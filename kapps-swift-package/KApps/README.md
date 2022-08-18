# `kapps` Swift Package

This Swift package contains the kapps libraries as binary frameworks.  Swift PM can't build the cross-platform source of the `kapps` libraries, so we build it in CI and publish binary frameworks.

Builds are checked into a Swift package repository so they can be referenced like any other Swift package, including tagged versions and branches.

## Tagged versions

CI builds from `kapps-#.#.#` tags in the source repo automatically publish the new build to the `kapps-swift` repository and tag it as the specified version.  This should be done manually on the source repository's `master` branch when libraries are ready for release.  (These serve as releases for all platforms, not just the Swift packages.)

## Branches

Builds from the source repo's `master` branch are pushed to `main` in the Swift package repository (the Swift package repo picked up the latest default branch name, but the source repo hasn't been changed yet).  You can reference the `main` branch to get the latest work that has been merged, but not released.

Feature branches are not published by default, but can be published by adding the feature branch's name in the publish script.  For projects that require work in both the `kapps` libraries and downstream projects, opt in to feature branch publishing, then reference the feature branch in downstream projects.
