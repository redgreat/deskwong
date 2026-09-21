FROM golang:1.24-alpine AS build
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
EXPOSE 8000
ENV DESKWONG_CONFIG=/app/conf/config.yml
ENTRYPOINT ["/app/deskwong-service"]
