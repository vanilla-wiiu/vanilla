# Docker for building

Since Vanilla's infrastructure can be somewhat complicated (and to aid in auto-building), there are Docker images for all supported platforms.

Usage is as follows for all platforms:

- First, build the Docker image. We give it the tag `vanilla-build:latest` but you can choose anything.

  ```
  docker build -t vanilla-build:latest .
  ```

- Secondly, use the new Docker image to build. Two volumes must be mounted, `/vanilla` for the source directory, and `/install` for the final compiled file structure. The following paths (`~/vanilla-src` and `~/vanilla-build`) are provided as an example, but you should select paths that make the most sense on your system.

  ```
  docker run -v ~/vanilla-src:/vanilla -v ~/vanilla-build:/install vanilla-build:latest
  ```

Docker should succeed, and the end result should be a ready-to-use application.