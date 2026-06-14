FROM ubuntu:noble AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG PICO_SDK_REF=2.2.0
ENV PICO_SDK_PATH=/opt/pico-sdk

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        gcc-arm-none-eabi \
        git \
        libnewlib-arm-none-eabi \
        make \
        python3 \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch "${PICO_SDK_REF}" \
        --recurse-submodules --shallow-submodules \
        https://github.com/raspberrypi/pico-sdk.git "${PICO_SDK_PATH}"

COPY firmware/ /src/firmware/

RUN set -eu; \
    for board in pico pico2; do \
        cmake -S /src/firmware -B "/build/${board}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DPICO_BOARD="${board}"; \
        cmake --build "/build/${board}" --parallel 4; \
        mkdir -p /builds; \
        cp "/build/${board}/xvcPico.uf2" "/builds/xvcPico-${board}.uf2"; \
    done

FROM scratch AS artifacts
COPY --from=builder /builds/ /builds/
