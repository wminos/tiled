// Editor-only verification/authoring utility using the upstream AutoMapper API.
#include "automapper.h"
#include "mapdocument.h"
#include "mapreader.h"
#include "mapwriter.h"
#include "tilelayer.h"
#include "tile.h"
#include "tileset.h"
#include <QApplication>
#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <stdexcept>

using namespace Tiled;

static void require(bool ok, const QString &message)
{
    if (!ok)
        throw std::runtime_error(message.toStdString());
}

static TileLayer *layer(Map *map, const QString &name)
{
    for (auto *item : map->layers())
        if (item->name() == name)
            return item->asTileLayer();
    throw std::runtime_error("Missing layer: " + name.toStdString());
}

static QByteArray snapshot(Map *map)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    MapWriter().writeMap(map, &buffer);
    return bytes;
}

static QByteArray protectedSnapshot(Map *map)
{
    auto copy = map->clone();
    for (int i=copy->layerCount()-1;i>=0;--i) {
        const auto name=copy->layerAt(i)->name();
        if (name=="Walls" || name=="CaveRoof" || name=="CaveUpper" || name=="CaveLower")
            delete copy->takeLayerAt(i);
    }
    return snapshot(copy.get());
}

static void apply(MapDocument &doc, const QString &rules)
{
    // Follow upstream tests/automapping and AutoMapperWrapper's full-map sequence.
    // This runner deliberately accepts only a flat rules.txt of rule-map paths.
    QFile list(rules);
    require(list.open(QIODevice::ReadOnly),list.errorString());
    std::vector<std::unique_ptr<AutoMapper>> mappers;
    for (const auto &line : list.readAll().split('\n')) {
        const auto file=QString::fromUtf8(line).trimmed();
        if (file.isEmpty() || file.startsWith('#')) continue;
        require(file.endsWith(".tmx"),"Runner expects a flat list of TMX rule maps");
        MapReader reader;
        auto rule=reader.readMap(QFileInfo(rules).dir().filePath(file));
        require(bool(rule),reader.errorString());
        auto mapper=std::make_unique<AutoMapper>(std::move(rule));
        require(mapper->errorString().isEmpty(),mapper->errorString());
        require(mapper->warningString().isEmpty(),mapper->warningString());
        mappers.push_back(std::move(mapper));
    }
    require(!mappers.empty(),"Empty rules list");
    AutoMappingContext context(&doc);
    for (auto &mapper : mappers) mapper->prepareAutoMap(context);
    QRegion region(0,0,doc.map()->width(),doc.map()->height());
    for (auto &mapper : mappers) mapper->autoMap(region,nullptr,context);
    require(context.newLayers.empty(),"Create the cave output layers before Automapping");
    for (auto &[original,output] : context.originalToOutputLayerMapping) {
        const auto diff=original->computeDiffRegion(*output);
        original->setCells(0,0,output.get(),diff);
    }
}

static void audit(Map *map)
{
    auto *shape = layer(map, "CaveShape");
    auto *walls = layer(map, "Walls");
    auto *roof = layer(map, "CaveRoof");
    auto *upper = layer(map, "CaveUpper");
    auto *lower = layer(map, "CaveLower");
    const auto rock = [&](int x, int y) { return shape->contains(x,y) && !shape->cellAt(x,y).isEmpty(); };
    const auto check = [&](TileLayer *output, int x, int y, bool expected, const char *part, int value) {
        const auto cell = output->cellAt(x,y);
        require(cell.isEmpty() != expected, QString("Wrong occupancy in %1 at %2,%3").arg(output->name()).arg(x).arg(y));
        if (expected) {
            require(cell.tile()->property("cavePart").toString() == part &&
                    cell.tile()->property(QString(part) == "roof" ? "neighborMask" : "edgeVariant").toInt() == value,
                    QString("Wrong cave tile in %1 at %2,%3").arg(output->name()).arg(x).arg(y));
        }
    };
    for (int y = 0; y < map->height(); ++y) {
        for (int x = 0; x < map->width(); ++x) {
            require(walls->cellAt(x,y).isEmpty() != rock(x,y), "Collision differs from authored footprint");
            // Independent spatial oracle. Diagonal notches matter only between two rock edges.
            const int sy = y+2;
            const bool n=rock(x,sy-1), e=rock(x+1,sy), s=rock(x,sy+1), w=rock(x-1,sy);
            const int mask = n | e<<1 | s<<2 | w<<3 | (n&&e&&rock(x+1,sy-1))<<4 |
                             (s&&e&&rock(x+1,sy+1))<<5 | (s&&w&&rock(x-1,sy+1))<<6 | (n&&w&&rock(x-1,sy-1))<<7;
            check(roof,x,y,rock(x,sy),"roof",mask);
            for (int offset : {0,1}) {
                const int base = y+offset;
                const int left = !rock(x-1,base) ? 1 : rock(x-1,base+1) ? 2 : 0;
                const int right = !rock(x+1,base) ? 1 : rock(x+1,base+1) ? 2 : 0;
                check(offset ? upper : lower,x,y,rock(x,base)&&!rock(x,base+1),offset ? "upper":"lower",3*left+right);
            }
        }
    }
    for (auto *output : {walls,roof,upper,lower})
        require((output->region() - QRegion(0,0,map->width(),map->height())).isEmpty(), "Output outside finite map");
}

static void regression(MapDocument &doc, const QString &rules)
{
    const auto protectedBefore = protectedSnapshot(doc.map());
    apply(doc, rules);
    audit(doc.map());
    require(protectedSnapshot(doc.map()) == protectedBefore, "Automapping changed authored layers or objects");
    const auto once = snapshot(doc.map());
    apply(doc, rules);
    require(snapshot(doc.map()) == once, "Repeated Automapping changed the map");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // Isolated application identity: no reads/writes to the user's Tiled preferences.
    app.setOrganizationName("OneKeyOutTests");
    app.setApplicationName("NativeAutomapping");
    try {
        const auto args = app.arguments();
        require(args.size() == 4 && (args[1] == "--check" || args[1] == "--write"),
                "Usage: onekeyout-automap --check|--write MAP RULES");
        const QString path = QFileInfo(args[2]).absoluteFilePath();
        const QString rules = QFileInfo(args[3]).absoluteFilePath();
        MapReader reader;
        auto map = reader.readMap(path);
        require(bool(map), reader.errorString());
        const auto authored = snapshot(map.get());
        map->fileName=path;
        MapDocument doc(std::move(map));
        regression(doc,rules);
        if (args[1] == "--check") {
            require(snapshot(doc.map()) == authored, "Saved map is stale; run native Automapping and save");
        }
        // Remove, restore, and open a doorway: stale shifted output must disappear.
        auto *shape = layer(doc.map(),"CaveShape");
        const auto original = snapshot(doc.map());
        std::unique_ptr<TileLayer> savedShape(shape->clone());
        for (int y=2; y<doc.map()->height(); ++y)
            for (int x=0; x<doc.map()->width(); ++x)
                if ((x+2*y)%5 == 0)
                    shape->setCell(x,y,Cell());
        apply(doc,rules);
        audit(doc.map());
        shape->setCells(0,0,savedShape.get(),QRegion(0,0,doc.map()->width(),doc.map()->height()));
        apply(doc,rules);
        require(snapshot(doc.map()) == original, "Restore did not recreate identical walls");
        // Exercise all 256 neighborhoods, including corners, thin walls and map edges.
        auto fixture = std::make_unique<Map>(Map::Orthogonal,96,98,32,32);
        Cell marker;
        for (const auto &tileset : doc.map()->tilesets())
            for (const auto *tile : tileset->tiles())
                if (tile->property("caveSolid").toBool()) marker=Cell(tileset.data(),tile->id());
        require(!marker.isEmpty(), "Missing caveSolid marker in the attached tilesets");
        fixture->addTileset(marker.tileset()->sharedFromThis());
        for (const auto &name : {"CaveShape","Walls","CaveRoof","CaveUpper","CaveLower"})
            fixture->addLayer(std::make_unique<TileLayer>(name,0,0,96,98));
        auto *input = layer(fixture.get(),"CaveShape");
        const QPoint offsets[]{{0,-1},{1,0},{0,1},{-1,0},{1,-1},{1,1},{-1,1},{-1,-1}};
        for (int mask=0; mask<256; ++mask) {
            const QPoint center((mask%16)*6+2,(mask/16)*6+4);
            input->setCell(center.x(),center.y(),marker);
            for (int bit=0; bit<8; ++bit)
                if (mask & (1<<bit)) input->setCell(center.x()+offsets[bit].x(),center.y()+offsets[bit].y(),marker);
        }
        input->setCell(0,2,marker);
        input->setCell(95,97,marker);
        MapDocument fixtureDoc(std::move(fixture));
        regression(fixtureDoc,rules);
        if (args[1] == "--write") {
            MapWriter writer;
            require(writer.writeMap(doc.map(),path),writer.errorString());
        }
        qInfo() << "PASS: native rules, 256 neighborhoods, map edges, regeneration, erase/restore, unchanged gameplay layers";
        return 0;
    } catch (const std::exception &error) {
        qCritical().noquote() << error.what();
        return 1;
    }
}
