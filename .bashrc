# Site runtime
sh <(curl -fsSL https://deno.land/install.sh) '-y' > /dev/null 2>&1
export PATH="/home/$USER/.deno/bin/:$PATH"

# ------------------------------------------------------------------------------

# Tasklet CLI
if [ ! -e "/usr/local/bin/tasklet" ]; then
    sudo ln -s ~/workspace/tasklet /usr/local/bin/tasklet
fi

# ------------------------------------------------------------------------------

# Login
cat <<EOF
Welcome, $USER!
===========================
Type 'exit' to exit =)
EOF
