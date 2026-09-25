FROM golang:1.25-alpine AS build
WORKDIR /src
COPY service/go.mod service/go.sum ./
RUN go mod download
COPY service/ ./
ARG VERSION=dev
RUN CGO_ENABLED=0 GOOS=linux go build -trimpath \
    -ldflags "-s -w -X main.version=${VERSION}" -o /out/deskwong-service .

FROM gcr.io/distroless/static-debian12:nonroot
WORKDIR /app
COPY --from=build /out/deskwong-service /app/deskwong-service
COPY --chown=nonroot:nonroot conf/config.yml.sample /app/conf/config.yml
COPY --chown=nonroot:nonroot conf/worktime.sql /app/conf/worktime.sql
COPY --chown=nonroot:nonroot service/data/ /app/data/
EXPOSE 8001
ENV DESKWONG_CONFIG=/app/conf/config.yml
ENV DESKWONG_SETTINGS_DB=/app/data/settings.sqlite
ENTRYPOINT ["/app/deskwong-service"]
