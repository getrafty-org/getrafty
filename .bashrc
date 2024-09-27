# Add Node
if ! command -v node >/dev/null 2>&1; then
    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh 2>/dev/null | bash >/dev/null 2>&1
    export NVM_DIR="$HOME/.nvm"
    [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" >/dev/null 2>&1 # This loads nvm
    nvm install --lts >/dev/null 2>&1
    echo 'Devvm init complete'
fi

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
Hi, $(whoami)!
Welcome to devvm container!
(___-){
===================================

Type 'exit' to exit =)
EOF
