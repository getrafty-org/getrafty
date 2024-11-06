#!/usr/bin/env python3
import os
import subprocess

import click

CONTAINER_NAME = "getrafty"
CONTAINER_USER = "ubuntu"
CONTAINER_BIND_DIR = os.getcwd()
CONTAINER_SSH_PORT = 3333
CONTAINER_SITE_PORT = 3000


def error_exit(message):
    """Print an error message and exit."""
    click.echo(f"Error: {message}", err=True)
    exit(1)


def check_docker_is_installed():
    """Check if Docker is installed."""
    if subprocess.call(["docker", "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
        error_exit("Docker is not installed. Please install Docker first.")


def check_container_is_up():
    """Check if the DevVM is running."""
    result = subprocess.run(
        ["docker", "ps", "--filter", f"name={CONTAINER_NAME}", "--format", "{{.ID}}"],
        capture_output=True,
        text=True,
    )
    container_id = result.stdout.strip()
    if container_id:
        click.echo(f"Container {container_id} is already running on port {CONTAINER_SSH_PORT}.")
        if click.confirm("Would you like to shutdown it?"):
            subprocess.run(["docker", "stop", container_id])
            subprocess.run(["docker", "rm", container_id])
            return False
        return True
    return False


@click.group()
def cli():
    """Tasklet"""
    pass


@cli.command()
@click.option("--build", is_flag=True, help="Rebuild the container image.")
def boot(build):
    """Build and run the development VM."""
    check_docker_is_installed()

    if check_container_is_up():
        click.echo("Container is up.")
        return

    if build:
        result = subprocess.run(["docker", "images", CONTAINER_NAME, "-q"], capture_output=True, text=True)
        image_id = result.stdout.strip()
        if image_id:
            subprocess.run(["docker", "rmi", image_id], check=True)
        subprocess.run(["docker", "build", "-t", CONTAINER_NAME, "."], check=True)

    try:
        subprocess.run(
            [
                "docker", "run", "-d",
                "--name", CONTAINER_NAME,
                "--cap-add", "SYS_PTRACE",
                "--cap-add", "SYS_ADMIN",
                "--privileged",
                "--device", "/dev/fuse:/dev/fuse",
                "-v", f"{CONTAINER_BIND_DIR}:/home/{CONTAINER_USER}/workspace/",
                "-p", f"{CONTAINER_SSH_PORT}:22",
                "-p", f"{CONTAINER_SITE_PORT}:{CONTAINER_SITE_PORT}",
                CONTAINER_NAME,
            ],
            check=True,
        )

        subprocess.run(
            ["docker", "exec", CONTAINER_NAME, "/bin/sh", "-c", f"mkdir -p /home/{CONTAINER_USER}/.tasklet"],
            check=True,
        )
        subprocess.run(
            ["docker", "cp", ".bashrc", f"{CONTAINER_NAME}:/home/{CONTAINER_USER}/.tasklet/.bashrc"],
            check=True,
        )
        subprocess.run(
            [
                "docker", "exec", CONTAINER_NAME, "/bin/sh", "-c",
                f"cat /home/{CONTAINER_USER}/.tasklet/.bashrc >> /home/{CONTAINER_USER}/.bashrc",
            ],
            check=True,
        )

        click.echo("Connect by running: `tasklet attach`")
    except subprocess.CalledProcessError as e:
        error_exit(f"Failed to start container: {e}")


@cli.command()
@click.option("--root", is_flag=True, help="Attach with root access.")
def attach(root):
    """Attach to the running DevVM."""
    result = subprocess.run(
        ["docker", "ps", "--filter", f"name={CONTAINER_NAME}", "--format", "{{.ID}}"],
        capture_output=True,
        text=True,
    )
    container_id = result.stdout.strip()

    if not container_id:
        error_exit("DevVM is not running.")

    if root:
        subprocess.run(["docker", "exec", "-it", container_id, "/bin/bash"], check=True)
    else:
        ip_result = subprocess.run(
            ["hostname", "-I"],
            capture_output=True,
            text=True,
        )
        host_ip = ip_result.stdout.split()[0]
        subprocess.run(["ssh", f"{CONTAINER_USER}@{host_ip}", "-p", str(CONTAINER_SSH_PORT)])


if __name__ == "__main__":
    cli()
