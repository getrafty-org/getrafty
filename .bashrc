# Deno
sh <(curl -fsSL https://deno.land/install.sh) '-y' > /dev/null 2>&1

# ------------------------------------------------------------------------------

# Clippy
touch ~/.clippy
if [ ! -e "/usr/local/bin/clippy" ]; then
    sudo ln -s ~/workspace/clippy.sh /usr/local/bin/clippy
    sudo chmod +x /usr/local/bin/clippy
fi

# ------------------------------------------------------------------------------

# Welcome
cat <<EOF
Welcome to devvm container.
===========================
Type 'exit' to exit =)
EOF
