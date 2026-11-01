FROM golang:1.25-alpine AS build
WORKDIR /src
COPY service/go.mod service/go.sum ./
RUN go mod download
COPY service/ ./
ARG VERSION=dev
RUN CGO_ENABLED=0 GOOS=linux go build -trimpath \
    -ldflags "-s -w -X main.version=${VERSION}" -o /out/deskwong-service .

# 同一份二进制构建两个镜像：
#   deskwong     MODE=full（默认，单镜像全量接口；国内机 compose 里再切 core）
#   deskwong-ai  MODE=ai（国外 VPS，只暴露 /health 与 /ai/usage）
FROM gcr.io/distroless/static-debian12:nonroot
ARG MODE=full
ARG CONFIG_SAMPLE=config.yml.sample
WORKDIR /app
COPY --from=build /out/deskwong-service /app/deskwong-service
COPY --chown=nonroot:nonroot conf/${CONFIG_SAMPLE} /app/conf/config.yml
COPY --chown=nonroot:nonroot service/data/ /app/data/
EXPOSE 8001
ENV DESKWONG_CONFIG=/app/conf/config.yml
ENV DESKWONG_DB=/app/data/deskwong.sqlite
ENV DESKWONG_MODE=${MODE}
ENTRYPOINT ["/app/deskwong-service"]
