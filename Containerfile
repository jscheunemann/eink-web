# Known-good build environment (LLVM 18 for Emscripten 3.1.51).
#
#   podman build -t eink-web-build .
#   podman run --rm -v "$PWD":/src:Z eink-web-build
#
# The repo is mounted at /src; .work/, build/ and dist/ land in the checkout.
FROM docker.io/library/ubuntu:24.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates git cmake make g++ clang-18 lld-18 llvm-18 \
      python3 python3-yaml nodejs npm \
 && rm -rf /var/lib/apt/lists/*

ENV LLVM_ROOT=/usr/lib/llvm-18/bin
WORKDIR /src
CMD ["sh", "-c", "tools/bootstrap.sh && tools/build.sh"]
