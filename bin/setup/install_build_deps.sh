echo "🔍 Checking for required build dependencies..."

sudo apt update

# -- Meson/Ninja --
if ! command -v meson &>/dev/null || ! command -v ninja &>/dev/null; then
    echo "🛠️ Installing Meson and Ninja build system..."
    sudo apt install -y meson ninja-build
else
    echo "✅ Meson and Ninja already installed."
fi

# -- LibMagic --
if ! dpkg -l | grep -q libmagic1; then
    echo "🔌 Installing libmagic1..."
    sudo apt install -y libmagic1
else
    echo "✅ libmagic1 already installed."
fi

if ! dpkg -l | grep -q libmagic-dev; then
    echo "🔌 Installing libmagic-dev..."
    sudo apt install -y libmagic-dev
else
    echo "✅ libmagic-dev already installed."
fi

# -- PostgreSQL client + libpqxx --
if ! dpkg -l | grep -q libpqxx-dev; then
    echo "🔌 Installing libpqxx..."
    sudo apt install -y libpqxx-dev
else
    echo "✅ libpqxx-dev already installed."
fi

# -- libsodium --
if ! dpkg -l | grep -q libsodium-dev; then
    echo "🔌 Installing libsodium-dev..."
    sudo apt install -y libsodium-dev
else
    echo "✅ libsodium-dev already installed."
fi

# -- libcurl --
if ! dpkg -l | grep -q libcurl4-openssl-dev; then
    echo "🔌 Installing libcurl4-openssl-dev..."
    sudo apt install -y libcurl4-openssl-dev
else
    echo "✅ libcurl4-openssl-dev already installed."
fi

# -- uuid --
if ! dpkg -l | grep -q uuid-dev; then
    echo "🔌 Installing uuid-dev..."
    sudo apt install -y uuid-dev
else
    echo "✅ uuid-dev already installed."
fi

# -- FUSE3 --
if ! dpkg -l | grep -q libfuse3-dev; then
    echo "🔌 Installing libfuse3-dev..."
    sudo apt install -y libfuse3-dev
else
    echo "✅ libfuse3-dev already installed."
fi

# -- yaml-cpp --
if ! dpkg -l | grep -q libyaml-cpp-dev; then
    echo "🔌 Installing libyaml-cpp-dev..."
    sudo apt install -y libyaml-cpp-dev
else
    echo "✅ libyaml-cpp-dev already installed."
fi

# -- pugixml --
if ! dpkg -l | grep -q libpugixml-dev; then
    echo "🔌 Installing libpugixml-dev..."
    sudo apt install -y libpugixml-dev
else
    echo "✅ libpugixml-dev already installed."
fi

# -- gtest --
if ! dpkg -l | grep -q libgtest-dev; then
    echo "🔌 Installing libgtest-dev..."
    sudo apt install -y libgtest-dev
else
    echo "✅ libgtest-dev already installed."
fi

# -- Boost (filesystem + system) --
if ! dpkg -l | grep -q libboost-filesystem-dev; then
    echo "🔌 Installing Boost (filesystem + system)..."
    sudo apt install -y libboost-filesystem-dev libboost-system-dev
else
    echo "✅ Boost components already installed."
fi

# -- libspdlog --
if ! dpkg -l | grep -q libspdlog-dev; then
    echo "🔌 Installing libspdlog-dev..."
    sudo apt install -y libspdlog-dev
else
    echo "✅ libspdlog-dev already installed."
fi
