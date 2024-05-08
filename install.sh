cp cmd/kcpcmd /bin/
cp fpg/kcpfpg /bin/
cp tcpborphserver3/tcpborphserver3 /bin/
# create startup service file
echo "Description=TCPBorphServer allows programming and communication with the FPGA
Wants=network.target
After=syslog.target network-online.target
[Service]
Type=simple
ExecStart=/bin/tcpborphserver3 -f
Restart=on-failure
RestartSec=10
KillMode=process
[Install]
WantedBy=multi-user.target" > /etc/systemd/system/tcpborphserver.service
# reload services
systemctl daemon-reload
# enable the service
systemctl enable tcpborphserver
# start the service
systemctl start tcpborphserver
# check the status of your service
systemctl status tcpborphserver
