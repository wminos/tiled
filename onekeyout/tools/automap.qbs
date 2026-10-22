import qbs.FileInfo

CppApplication {
    name: "onekeyout-automap"
    builtByDefault: false
    install: false
    consoleApplication: true
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory, "bin")
    Depends { name: "libtilededitor" }
    Depends { name: "Qt.widgets" }
    cpp.cxxLanguageVersion: "c++17"
    cpp.warningLevel: "all"
    files: ["automap.cpp"]
}
