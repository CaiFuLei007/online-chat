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

# 使用阿里云镜像加速
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libjsoncpp-dev uuid-dev libssl-dev zlib1g-dev \
        libmariadb-dev libhiredis-dev curl \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 安装 Drogon（从源码编译，显式启用 MySQL 和 Redis 支持）
ARG DROGON_VERSION=v1.9.13
# git clone 加重试，应对不稳定的 GitHub 访问
RUN for i in 1 2 3 4 5; do \
        rm -rf /tmp/drogon && \
        git clone --depth 1 --branch ${DROGON_VERSION} \
            https://github.com/drogonframework/drogon.git /tmp/drogon && \
        cd /tmp/drogon && \
        git submodule update --init --recursive && break || \
        { echo "clone attempt $i failed, retrying..."; sleep 5; }; \
    done \
    && cd /tmp/drogon \
    && cmake -B build -DCMAKE_BUILD_TYPE=Release \
             -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF \
             -DBUILD_MYSQL=ON \
             -DBUILD_REDIS=ON \
    && cmake --build build -j$(nproc) \
    && cmake --install build \
    && rm -rf /tmp/drogon

# 系统安装 nlohmann_json，避免 jwt-cpp 配置时从 GitHub 下载（网络不稳会导致构建失败）
# 注：放在 Drogon 层之后以保留其构建缓存
RUN apt-get update && apt-get install -y --no-install-recommends \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# 编译 online-chat
WORKDIR /app
COPY CMakeLists.txt ./
COPY src/ src/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

# ---- runtime：运行阶段 ----
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# 使用阿里云镜像加速
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y --no-install-recommends \
        libjsoncpp25 libssl3 zlib1g \
        libmariadb3 libhiredis0.14 curl \
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
