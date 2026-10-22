import qbs.FileInfo

CppApplication {
    name: "test-onekeyout"
    builtByDefault: false
    install: false
    installDebugInformation: false
    consoleApplication: true
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory, "bin")
    Depends { name: "libtiled" }
    Depends { name: "Qt.widgets" }
    Depends { name: "Qt.testlib" }
    cpp.cxxLanguageVersion: "c++17"
    cpp.warningLevel: "all"
    cpp.includePaths: ["../src"]
    files: ["test_game.cpp", "../src/*.h", "../src/*.cpp", "../assets.qrc"]
    excludeFiles: ["../src/main.cpp"]
}
