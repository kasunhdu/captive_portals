

#Captive Portal for Any Device using ESP32

This project implements a captive portal system using an ESP32, allowing any Wi-Fi-enabled device to automatically connect and be redirected to a custom web interface without requiring manual IP entry. The system creates a soft access point (AP) and intercepts DNS requests to force all connected clients to a local web server.

It is useful for IoT configuration, device setup interfaces, or offline web-based control systems.

Features:

Automatic redirection (captive portal behavior)
DNS spoofing to capture all requests
Embedded web server for UI/control
Works with smartphones, laptops, and other Wi-Fi devices
No internet connection required

to generate QR CODE use this text on qr generator - WIFI:S:Connect-To-Portal;T:WPA;P:12345678;;

<img width="286" height="234" alt="WIFI_qr" src="https://github.com/user-attachments/assets/7ca9d1a6-23ab-4952-a91f-7a243dfac89b" />


