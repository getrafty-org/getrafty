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


def check_container_exists():
    """Check if a container with the same name exists."""
    result = subprocess.run(
        ["docker", "ps", "-a", "--filter", f"name={CONTAINER_NAME}", "--format", "{{.ID}}"],
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def check_container_is_running():
    """Check if the container is running."""
    result = subprocess.run(
        ["docker", "ps", "--filter", f"name={CONTAINER_NAME}", "--format", "{{.ID}}"],
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def remove_existing_container():
    """Remove an existing container."""
    container_id = check_container_exists()
    if container_id:
        click.echo(f"Removing existing container {container_id}...")
        subprocess.run(["docker", "rm", "-f", container_id], check=True)


def restart_existing_container():
    """Restart a stopped container."""
    container_id = check_container_exists()
    if container_id:
        click.echo(f"Found existing container {container_id}. Restarting it...")
        subprocess.run(["docker", "start", container_id], check=True)
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

    if build:
        # Handle rebuild case
        if check_container_is_running():
            if click.confirm("The container is running. Do you want to stop and rebuild it?"):
                remove_existing_container()
            else:
                click.echo("Exiting without rebuilding.")
                return
        else:
            remove_existing_container()

        # Remove image and rebuild
        result = subprocess.run(["docker", "images", CONTAINER_NAME, "-q"], capture_output=True, text=True)
        image_id = result.stdout.strip()
        if image_id:
            subprocess.run(["docker", "rmi", image_id], check=True)
        subprocess.run(["docker", "build", "-t", CONTAINER_NAME, "."], check=True)
    else:
        # Handle reuse case
        if check_container_is_running():
            click.echo("Container is up. Connect by running: `tasklet attach`.")
            return

        if restart_existing_container():
            click.echo("Container restarted successfully. Connect by running: `tasklet attach`.")
            return

    try:
        # Start the container
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

        click.echo("Connect by running: `tasklet attach`.")
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
