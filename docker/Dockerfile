# ============================================================================
# online-chat 应用镜像（多阶段构建）
#
# 构建：docker build -f docker/Dockerfile -t online-chat:dev .
# 运行：docker run -p 8080:8080 -e JWT_SECRET=xxx online-chat:dev
#
# 说明：
#   - Drogon 从源码编译安装（builder 阶段）
#   - 运行阶段仅复制编译产物与资源，镜像精简
#   - 所有敏感配置通过环境变量注入，config.json 不含密钥
# ============================================================================

# ---- builder：编译阶段 ----
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libjsoncpp-dev uuid-dev libssl-dev zlib1g-dev \
        libmysqlclient-dev libhiredis-dev curl \
    && rm -rf /var/lib/apt/lists/*

# 安装 Drogon（从源码编译）
ARG DROGON_VERSION=v1.9.13
RUN git clone --depth 1 --branch ${DROGON_VERSION} \
        https://github.com/drogonframework/drogon.git /tmp/drogon \
    && cd /tmp/drogon \
    && git submodule update --init --recursive \
    && cmake -B build -DCMAKE_BUILD_TYPE=Release \
             -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF \
    && cmake --build build -j$(nproc) \
    && cmake --install build \
    && rm -rf /tmp/drogon

# 编译 online-chat
WORKDIR /app
COPY CMakeLists.txt ./
COPY src/ src/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# ---- runtime：运行阶段 ----
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libjsoncpp25 libssl3 zlib1g \
        libmysqlclient21 libhiredis0.14 curl \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# 复制编译产物与资源
COPY --from=builder /app/build/online_chat ./
COPY config/ ./config/
COPY frontend/ ./frontend/
COPY sql/ ./sql/

EXPOSE 8080

CMD ["./online_chat"]
