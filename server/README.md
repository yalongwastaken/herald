# herald — server setup

> All commands in this guide are run on the Raspberry Pi 5 over SSH unless stated otherwise.

The RPi 5 is the central node. It runs Whisper (ASR), an LLM, Kokoro (TTS), the MQTT broker, and
the WiFi hotspot. Everything is set up once over SSH while the Pi is on a local network, after
which the Pi is self-contained and auto-starts herald on boot. Once the hotspot is configured, the
`herald` WiFi network is broadcast and all future SSH access is via `herald@192.168.4.1` from any
machine connected to that network.

## First-time setup order

1. SSH into the Pi while it is on a local network (home router or direct ethernet)
2. Clone the repo and set up the Python environment
3. Download models
4. Configure the systemd service
5. Set up the WiFi hotspot
6. Configure Mosquitto to bind to the hotspot interface
7. Reboot — Pi broadcasts `herald`, herald auto-starts on boot
8. Flash ESP32 nodes locally on your machine (see [`firmware/README.md`](../firmware/README.md)) —
   nodes have `herald`/`herald1234` hardcoded and connect automatically
9. _(Optional)_ Connect to `herald` WiFi and SSH to `192.168.4.1` to monitor

---

## 1. Clone the repo

**Run on the Pi:**

```bash
git clone https://github.com/yalongwastaken/herald.git
cd herald
```

---

## 2. Python environment

**Run on the Pi:**

```bash
python3.13 -m venv ~/herald-env
source ~/herald-env/bin/activate
pip install -r server/requirements.txt
```

---

## 3. Download models

**Run on the Pi:**

```bash
cd server
python3 download_models.py
```

Downloads the following into `server/models/`:

| File | Size |
|---|---|
| `Llama-3.2-3B-Instruct-Q4_K_M.gguf` | ~2.0 GB |
| `kokoro-v1.0.onnx` | ~310 MB |
| `voices-v1.0.bin` | ~740 MB |

Whisper `base.en` is downloaded automatically by `faster-whisper` on first run.

---

## 4. systemd service

**Run on the Pi:**

```bash
sudo nano /etc/systemd/system/herald.service
```

Paste:

```ini
[Unit]
Description=herald voice dispatch service
After=network.target mosquitto.service

[Service]
ExecStart=/home/herald/herald-env/bin/python3 /home/herald/herald/server/main.py
Restart=on-failure
Environment=PYTHONUNBUFFERED=1
User=herald
WorkingDirectory=/home/herald/herald/server

[Install]
WantedBy=multi-user.target
```

Register it:

```bash
sudo systemctl daemon-reload
sudo systemctl enable herald
```

Do not start it yet — Mosquitto and the hotspot must be configured first.

---

## 5. WiFi hotspot

**Run on the Pi.**

The Pi broadcasts the `herald` WiFi network using hostapd and dnsmasq. ESP32 nodes and development
machines connect directly — no external router needed. The SSID and password are hardcoded into
the ESP32 firmware, so nodes connect automatically on boot once this is configured.

| Parameter | Value |
|---|---|
| SSID | `herald` |
| Password | `herald1234` |
| Pi IP | `192.168.4.1` |
| DHCP range | `192.168.4.2 – 192.168.4.20` |
| SSH | `herald@192.168.4.1` |
| MQTT | `192.168.4.1:1883` |

**Install packages:**

```bash
sudo apt install -y hostapd dnsmasq
sudo systemctl stop hostapd
sudo systemctl stop dnsmasq
```

**Static IP for wlan0** — add to `/etc/dhcpcd.conf`:

```bash
sudo nano /etc/dhcpcd.conf
```

Add at the bottom:

```
interface wlan0
    static ip_address=192.168.4.1/24
    nohook wpa_supplicant
```

**dnsmasq config:**

```bash
sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.bak
sudo nano /etc/dnsmasq.conf
```

Add:

```
interface=wlan0
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
```

**hostapd config:**

```bash
sudo nano /etc/hostapd/hostapd.conf
```

Add:

```
interface=wlan0
driver=nl80211
ssid=herald
hw_mode=g
channel=6
country_code=US
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=herald1234
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
```

Register the config — edit `/etc/default/hostapd` and change `#DAEMON_CONF=""` to:

```
DAEMON_CONF="/etc/hostapd/hostapd.conf"
```

**Enable services:**

```bash
sudo systemctl unmask hostapd
sudo systemctl enable hostapd
sudo systemctl enable dnsmasq
```

**Hotspot startup service** — ensures the static IP is assigned before dnsmasq on every boot:

```bash
sudo nano /etc/systemd/system/herald-hotspot.service
```

Paste:

```ini
[Unit]
Description=Herald hotspot setup
After=dhcpcd.service hostapd.service
Requires=hostapd.service

[Service]
Type=oneshot
ExecStart=/sbin/ip addr add 192.168.4.1/24 dev wlan0
ExecStartPost=/bin/systemctl restart dnsmasq
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
```

Enable it:

```bash
sudo systemctl daemon-reload
sudo systemctl enable herald-hotspot
```

---

## 6. Mosquitto

**Run on the Pi.**

Mosquitto must be configured after the hotspot is set up since it binds to `192.168.4.1`.

```bash
sudo apt install mosquitto mosquitto-clients
sudo nano /etc/mosquitto/mosquitto.conf
```

Add:

```
listener 1883 192.168.4.1
listener 1883 127.0.0.1
allow_anonymous true
```

Enable Mosquitto:

```bash
sudo systemctl enable mosquitto
```

---

## 7. Reboot

**Run on the Pi:**

```bash
sudo reboot
```

After reboot the Pi broadcasts `herald` and herald starts automatically via systemd. Verify:

```bash
sudo systemctl status herald
sudo systemctl status hostapd
sudo systemctl status mosquitto
ip addr show wlan0   # should show inet 192.168.4.1/24
```

If `wlan0` is missing the IP:

```bash
sudo ip addr add 192.168.4.1/24 dev wlan0
sudo systemctl restart dnsmasq
```

---

## Running herald manually

Herald auto-starts on boot via systemd. To run manually:

**Run on the Pi:**

```bash
cd ~/herald/server
source ~/herald-env/bin/activate
python3 main.py
```

Herald will speak "Herald is online and ready." when the pipeline is up. Press and hold the
push-to-talk button (GPIO 17) to record, release to transcribe and dispatch.

---

## Monitoring (optional)

Connect your machine to the `herald` WiFi network, then SSH into the Pi:

```bash
ssh herald@192.168.4.1
```

View live logs:

```bash
sudo journalctl -u herald -n 50 -f
```

Test MQTT is reachable from the hotspot network:

```bash
# terminal 1 — subscribe to all herald topics
mosquitto_sub -h 192.168.4.1 -t "herald/#" -v

# terminal 2 — publish a test command
mosquitto_pub -h 192.168.4.1 -t "herald/cmd/node2" -m '{"tool":"buzz","arguments":{"duration_ms":500}}'
```

The message should appear in terminal 1 and the buzzer on node 2 should fire.