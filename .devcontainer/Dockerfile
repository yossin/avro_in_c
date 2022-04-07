# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.231.3/containers/alpine/.devcontainer/base.Dockerfile

# [Choice] Alpine version: 3.14, 3.13, 3.12, 3.11
ARG VARIANT="3.15"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-alpine-${VARIANT}



# ** [Optional] Uncomment this section to install additional packages. **
 RUN apk update \
     && apk add --no-cache g++ cmake gdb make meson ninja
RUN apk add --no-cache git librdkafka-dev librdkafka

# dependencies for dpdk
RUN apk add bsd-compat-headers linux-headers libexecinfo-dev py3-elftools libunwind libexecinfo libexecinfo-dev
RUN cd /tmp \
    && wget https://fast.dpdk.org/rel/dpdk-22.03.tar.xz   \
    && tar -xvf dpdk-22.03.tar.xz   \
    && cd dpdk-22.03 \
    && meson build \
    && cd build \
    && ninja \
    && ninja install


RUN apk add jansson-dev
RUN cd /tmp \
    && git clone -b release-1.10.2 https://github.com/apache/avro \
    && mkdir -p /tmp/avro/lang/c/build \
    && cd /tmp/avro/lang/c/build \
    && cmake ../ \
    && make install

