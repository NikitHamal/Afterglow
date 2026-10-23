set -e
mkdir -p /root/agb
cd /root/agb
[ -d blender-4.2.3-linux-x64 ] || (curl -sL -o blender.tar.xz https://download.blender.org/release/Blender4.2/blender-4.2.3-linux-x64.tar.xz && python3 -c "import tarfile;tarfile.open('blender.tar.xz').extractall('.', filter='data')" && rm blender.tar.xz && echo BLENDER_OK)
[ -f mpfb.zip ] || curl -sL -o mpfb.zip "https://extensions.blender.org/download/sha256:c075b3e9b94b8e5556a5f03f4653602d02ea306de08126e8f55ed6dee28a2dad/add-on-mpfb-v2.0.18.zip"
[ -d mh_assets ] || (curl -sL -o assets.zip http://files.makehumancommunity.org/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip && mkdir -p mh_assets && cd mh_assets && python3 -c "import zipfile;zipfile.ZipFile('../assets.zip').extractall('.')" && cd .. && rm assets.zip && echo ASSETS_OK)
B=/root/agb/blender-4.2.3-linux-x64/blender
$B -b --factory-startup --command extension install-file -r user_default -e mpfb.zip > ext.log 2>&1 && echo MPFB_OK
U=$HOME/.config/blender/4.2/extensions/.user/user_default/mpfb/data
mkdir -p $U
for d in /root/agb/mh_assets/*; do ln -sfn $d $U/$(basename $d); done
mkdir -p /home/marimo
ln -sfn $HOME/.config /home/marimo/.config   # pipeline scripts reference /home/marimo/.config/blender/... (where it was first built)
/root/agb/blender-4.2.3-linux-x64/4.2/python/bin/python3.11 -m ensurepip > /dev/null 2>&1 || true
/root/agb/blender-4.2.3-linux-x64/4.2/python/bin/python3.11 -m pip install -q pillow && echo PIL_OK
echo SETUP_DONE
