# Site runtime
sh <(curl -fsSL https://deno.land/install.sh) '-y' > /dev/null 2>&1
export PATH="/home/$USER/.deno/bin/:$PATH"

# ------------------------------------------------------------------------------

# Tasklet CLI
if [ ! -e "/usr/local/bin/tasklet" ]; then
    python3 -m venv ~/myenv
    source ~/myenv/bin/activate
    pip3 install -r ~/workspace/requirements.txt
    sudo ln -s ~/workspace/tasklet.py /usr/local/bin/tasklet
    sudo chmod +x /usr/local/bin/tasklet
else
    source ~/myenv/bin/activate
fi

# ------------------------------------------------------------------------------

# Login
cat <<EOF
Welcome, $USER!
===========================
Type 'exit' to exit =)
EOF
