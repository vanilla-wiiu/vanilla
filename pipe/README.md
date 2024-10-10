# `vanilla-pipe`

No, this is not something you smoke, `vanilla-pipe` is a program on various platforms that facilitates a connection between the Wii U and another device. Since the Wii U connection is slightly nonstandard, not all devices can connect to it on their own. Hence, `vanilla-pipe`, which allows a connection on one device that forwards all communication to another.

`vanilla-pipe` is also used locally to allow the connection to happen with root permissions (which the nonstandard connection requires) while the frontend can remain a user program (with regular access to the user's X/Wayland/PulseAudio sessions).
