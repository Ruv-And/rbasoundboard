# RBA Soundboard app

A web soundboard application that allows users to upload video/audio clips of funny moments, add titles and thumbnails, and play them back with real time audio effects (speed and pitch modifications). Built as a distributed microservices system. React frontend, Java Spring Boot API with PostgreSQL, C++ audio processing service using FFmpeg and Rubberband with gRPC for high-performance audio manipulation, and Apache Kafka for asynchronous activity tracking.


## Start everything
docker-compose up -d

## Stop everything
docker-compose down

## Rebuild after code changes
docker-compose up -d --build

## View logs for specific service
docker-compose logs -f api
docker-compose logs -f audio-processor
docker-compose logs -f frontend
docker-compose logs -f postgres

## Restart a specific service
docker-compose restart api

## Remove everything (including volumes/data)
docker-compose down -v

# Dev Workflow
## Rebuild API container
docker-compose up -d --build api

## Rebuild audio processor
docker-compose up -d --build audio-processor

## Changes auto-reload (hot reload enabled), Or rebuild:
docker-compose up -d --build frontend

docker-compose up --build