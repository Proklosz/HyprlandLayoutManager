#!/bin/bash

# Function to install dependencies and compile on Debian-based distributions (e.g., Ubuntu)
install_debian() {
    echo "Detected Debian-based distribution."
    sudo apt update
    sudo apt install -y build-essential gcc pkg-config libgtk-3-dev
    compile_program
}

# Function to install dependencies and compile on Fedora-based distributions
install_fedora() {
    echo "Detected Fedora-based distribution."
    sudo dnf install -y gcc pkg-config gtk3-devel
    compile_program
}

# Function to install dependencies and compile on Arch-based distributions
install_arch() {
    echo "Detected Arch-based distribution."
    sudo pacman -Syu --noconfirm
    sudo pacman -S --noconfirm gcc pkg-config gtk3
    compile_program
}

# Function to compile the C program
compile_program() {
    echo "Compiling the program..."
    gcc `pkg-config --cflags gtk+-3.0` da.c -o hyprlayout `pkg-config --libs gtk+-3.0` -lm
    if [ $? -eq 0 ]; then
        echo "Compilation successful. The binary 'hyprlayout' is ready."
        install_program
    else
        echo "Compilation failed."
        exit 1
    fi
}

# Function to install the binary and CSS file
install_program() {
    echo "Installing the binary and CSS file..."
    
    # Define installation paths
    BIN_PATH="/usr/local/bin"
    CSS_DIR="/usr/local/share/hyprlayout"
    CSS_FILE="h_d_a_css.css"
    DESKTOP_FILE="/usr/share/applications/hyprlayout.desktop"

    # Create the directory for the CSS file if it doesn't exist
    sudo mkdir -p "$CSS_DIR"
    
    # Copy the binary to /usr/local/bin
    sudo cp hyprlayout "$BIN_PATH"
    if [ $? -eq 0 ]; then
        echo "Binary 'hyprlayout' installed to $BIN_PATH."
    else
        echo "Failed to install binary."
        exit 1
    fi
    
    # Copy the CSS file to the appropriate directory
    sudo cp "$CSS_FILE" "$CSS_DIR"
    if [ $? -eq 0 ]; then
        echo "CSS file '$CSS_FILE' installed to $CSS_DIR."
    else
        echo "Failed to install CSS file."
        exit 1
    fi

    # Create and install the .desktop file
    echo "[Desktop Entry]
Type=Application
Name=Hyprlayout
Exec=$BIN_PATH/hyprlayout
Icon=utilities-terminal
Terminal=false
Categories=Utility;
Comment=A GTK-based application" | sudo tee $DESKTOP_FILE > /dev/null

    if [ $? -eq 0 ]; then
        echo ".desktop file installed to $DESKTOP_FILE."
    else
        echo "Failed to install .desktop file."
        exit 1
    fi

    echo "Installation completed successfully."
    echo "You can now run the program by typing 'hyprlayout' in the terminal."
}

# Detect the Linux distribution and install dependencies
if [ -f /etc/os-release ]; then
    . /etc/os-release
    case "$ID" in
        debian|ubuntu)
            install_debian
            ;;
        fedora)
            install_fedora
            ;;
        arch|manjaro)
            install_arch
            ;;
        *)
            echo "Unsupported distribution: $ID"
            exit 1
            ;;
    esac
else
    echo "Cannot detect the distribution. /etc/os-release file not found."
    exit 1
fi

