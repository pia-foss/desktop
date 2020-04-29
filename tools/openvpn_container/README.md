## PIA OpenVPN Docker Container

This is a simple Docker container used to test the client OpenVPN build.  This is only used for basic connectivity testing, it is not a complete client application.

#### Build

Building the container requires an out-of-dir resource (`../../deps/openvpn/linux/x86_64/pia-openvpn`). A small build script copies this file into a temporary directory, so `docker build` can use it and create the container.

```
$ bash build-container.sh
```

#### Run

Create a `.env` file in this directory (note that `OVPN_PORT` is optional. If not set, it defaults to `8080` for UDP and `500` for TCP):

```
OVPN_SERVER_IP=<server IP>
OVPN_SERVER_NAME=<server name>
OVPN_USERNAME=<your openvpn username>
OVPN_PASSWORD=<your openvpn password>
OVPN_PROTO=udp
OVPN_PORT=8080
```


And run your built container:

```
$ docker run --env-file .env --privileged pia-openvpn:latest
```
