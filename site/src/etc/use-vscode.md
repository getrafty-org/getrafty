---
layout: layout.vto
---

# Using Visual Studio Code

The container exposes an ssh port `:3333` so you can always connect under `ubuntu` user and use your preferred tool. This is a short how-to on setting up environment with VSCode.   

### Attach to dev VM
```shell
$ clippy attach
```


### Download VSCode server on the devvm

Install the VS Code CLI on the dev VM:

```shell
$ curl -Lk 'https://code.visualstudio.com/sha/download?build=stable&os=cli-alpine-x64' --output vscode_cli.tar.gz &\ 
     tar -xf vscode_cli.tar.gz
```

~Tip: it can be inconvenient to repeat this step multiple times. To streamline the process, you can add this command to your [.bashrc](https://github.com/sidosera/getrafty/blob/main/.bashrc) file, which will ensure the VSCode server is downloaded upon your first login.


### Start the tunnel
VSCode support tunneling right into container. Run the following command and follow command prompts:

```shell
$ ./code tunnel
# there will be several steps like gh auth, etc.
Open this link in your browser https://vscode.dev/tunnel/0000
```

### Connect to remote VSCode instance

The command below outputs the link to the newly created VSCode session like `https://vscode.dev/tunnel/0000` which let you open IDE in the browser.

### Recommended extensions

llvm-vs-code-extensions.vscode-clangd







