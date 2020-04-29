FROM ubuntu:bionic
# Install openvpn to pull in all the required dependencies. 
RUN apt-get update && apt-get install -y openvpn net-tools iputils-ping
COPY start.sh /root/start.sh
COPY openvpn_udp.ovpn /root/openvpn_udp.ovpn
COPY openvpn_tcp.ovpn /root/openvpn_tcp.ovpn
COPY temp/ /root/pia-openvpn
RUN chmod +x /root/start.sh
ENTRYPOINT ["/bin/bash"]
CMD ["/root/start.sh"]
