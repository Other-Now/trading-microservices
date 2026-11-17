# One image holds all four binaries; compose picks the command per service.
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake make libboost-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt .
COPY src src
COPY tests tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j"$(nproc)" \
    && ./build/unit_tests

FROM ubuntu:24.04
COPY --from=build /src/build/md_svc /src/build/risk_svc /src/build/gw_svc /src/build/loadgen /usr/local/bin/
