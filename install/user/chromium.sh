# Chrome ships in the base packages, so it never goes through
# quattro-install-browser, and fresh installs mark every migration as already
# applied. /etc/skel gives it its flags, but the native messaging hosts the
# bundled extensions talk to have to be registered here.
quattro-install-chromium-copy-url
quattro-install-chromium-ytdlp
