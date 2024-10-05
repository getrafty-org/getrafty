---
layout: layout.vto
---

# Using Visual Studio Code

### Attach to devvm
```bash
$ clippy attach
```


### Download VSCode server on the devvm

Download the vscode server on the devvm:

```bash
$ curl -Lk 'https://code.visualstudio.com/sha/download?build=stable&os=cli-alpine-x64' --output vscode_cli.tar.gz $ tar -xf vscode_cli.tar.gz
```

> /** *If you find yourself needing to frequently relaunch devvm, it can be inconvenient to repeat this step multiple times. To streamline the process, you can add this command to your [.bashrc](https://github.com/sidosera/getrafty/blob/main/.bashrc) file, which will ensure the VSCode server is downloaded upon your first login.* **/


### Start tunell
Run the following command and follow command prompts:

```bash
$ ./code tunnel
*
* Visual Studio Code Server
*
* By using the software, you agree to
* the Visual Studio Code Server License Terms (https://aka.ms/vscode-server-license) and
* the Microsoft Privacy Statement (https://privacy.microsoft.com/en-US/privacystatement).
*
✔ How would you like to log in to Visual Studio Code? · GitHub Account
To grant access to the server, please log into https://github.com/login/device and use code 0000-0000
✔ What would you like to call this machine? · XXXX
[2024-10-05 13:58:14] info Creating tunnel with the name: XXXX

Open this link in your browser https://vscode.dev/tunnel/XXXX

```

### Open VSCode session

The command below outputs the link to the newly created VSCode session, e.g. `https://vscode.dev/tunnel/XXXX`. Just open the link in your browser.


Enjoy coding.




