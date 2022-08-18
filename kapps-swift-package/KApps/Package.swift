// swift-tools-version:5.5

import PackageDescription

let package = Package(
    name: "KApps",
    products: [
        .library(
            name: "KApps",
            targets: ["kapps_core", "kapps_net", "kapps_regions"]
        ),
    ],
    dependencies: [
    ],
    targets: [
        .binaryTarget(name: "kapps_core", path: "kapps_core.xcframework"),
        .binaryTarget(name: "kapps_regions", path: "kapps_regions.xcframework"),
        .binaryTarget(name: "kapps_net", path: "kapps_net.xcframework"),
    ]
)
