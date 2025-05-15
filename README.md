# ft_trojan

`ft_shield` is a simple “Trojan-horse” style daemon that installs itself with root privileges and listens on port 4242. When a client connects and provides the correct password, it can spawn a root shell.

**Key features:**
- Installs a secondary copy in `/bin/ft_shield` and configures itself to start at boot  
- Runs as a background daemon  
- Supports up to 3 simultaneous client connections  
- Enforces a hard-coded password check before granting shell access  
