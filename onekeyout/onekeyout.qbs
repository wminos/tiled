import qbs.FileInfo

QtGuiApplication {
    name: "onekeyout"
    targetName: "One Key Out"
    version: "0.1.0"
    builtByDefault: false
    install: false
    installDebugInformation: false
    consoleApplication: false
    destinationDirectory: FileInfo.joinPaths(project.buildDirectory, "bin")
    Depends { name: "libtiled" }
    Depends { name: "Qt.widgets"; versionAtLeast: "6.2" }
    cpp.cxxLanguageVersion: "c++17"
    cpp.warningLevel: "all"
    bundle.identifierPrefix: "org.prototypes"
    bundle.infoPlist: ({
        "CFBundleIdentifier": "org.prototypes.onekeyout",
        "CFBundleDisplayName": "One Key Out",
        "NSHighResolutionCapable": true
    })
    cpp.rpaths: [FileInfo.joinPaths(cpp.rpathOrigin, "../Frameworks")]
    files: ["src/*.h", "src/*.cpp", "assets.qrc"]
}
