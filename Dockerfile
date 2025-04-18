FROM alpine:latest

RUN apk add --no-cache g++ make openssl-dev

WORKDIR /app
COPY . .

RUN make

EXPOSE 9000
CMD ["./proxy"]