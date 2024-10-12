# Deno runtime
sh <(curl -fsSL https://deno.land/install.sh) '-y' > /dev/null 2>&1
export PATH="/home/$USER/.deno/bin/:$PATH"

# ------------------------------------------------------------------------------

# Clippy
touch ~/.clippy
if [ ! -e "/usr/local/bin/clippy" ]; then
    sudo ln -s ~/workspace/clippy.sh /usr/local/bin/clippy
    sudo chmod +x /usr/local/bin/clippy
fi

# ------------------------------------------------------------------------------

# Login
cat <<EOF
Welcome to dev container.
===========================
Type 'exit' to exit =)
EOF
