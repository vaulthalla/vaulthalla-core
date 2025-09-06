#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::aku {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

std::string usage_provider() {
    const std::vector<std::string> providers = {
        "aws", "cloudflare-r2", "wasabi", "backblaze-b2", "digitalocean",
        "minio", "ceph", "storj", "other"
    };
    std::string options = "provider options: [";
    for (const auto& opt : providers) options += opt + " | ";
    options.pop_back(); // remove last space
    options.pop_back(); // remove last '|'
    options += "]";
    return options;
}

static std::shared_ptr<CommandUsage> list(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all API keys in the system.";
    cmd->optional_flags = {
        {"json", "Output the list in JSON format", {"--json", "-j"}}
    };
    cmd->examples = {
        {"vh api-keys", "List all API keys in the system."},
        {"vh api-key", "List all API keys in the system (using alias)."},
        {"vh aku", "List all API keys in the system (using shortest alias)."},
        {"vh api-keys --json", "List all API keys in JSON format."}
    };
    return cmd;
}

std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new API key for accessing S3 storage.";
    cmd->required = {
        {"api_key_name", "Name for the new API key", {"name", {"<name>"}}},
        {"access_key", "Access key for the S3 provider", {"access", {"<accessKey>"}}},
        {"secret_key", "Secret key for the S3 provider", {"secret", {"<secret>"}}},
        {"provider", "S3 provider (" + usage_provider() + ")", {"provider", {"<provider>"}}},
        {"endpoint", "Custom endpoint URL for the S3 provider (currently required for all providers)", {"endpoint", {"<endpoint>"}}}
    };
    cmd->optional = {
        {"region", "Region for the S3 provider", {"region", {"<region>"}}, "auto"}
    };
    cmd->examples = {
        {"vh api-key create --name mykey --access AKIA... --secret wJalrXUtnFEMI/K7MDENG/bPxRfiCYzEXAMPLEKEY --provider aws --region us-east-1",
         "Create a new API key named 'mykey' for AWS S3 in the us-east-1 region."},
        {"vh api-key mk --name r2key --access <accessKey> --secret <secret> --provider cloudflare-r2 --endpoint https://<account_id>.r2.cloudflarestorage.com",
         "Create a new API key named 'r2key' for Cloudflare R2 (using alias)."}
    };
    return cmd;
}

std::shared_ptr<CommandUsage> remove(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing API key by ID.";
    cmd->positionals = {{"api_key", "ID of the API key to delete", {"<id>", "<api_key_id>"}}};
    cmd->examples = {
        {"vh api-key delete 42", "Delete the API key with ID 42."},
        {"vh api-key rm 42", "Delete the API key with ID 42 (using alias)."}
    };
    return cmd;
}

std::shared_ptr<CommandUsage> info(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about an API key.";
    cmd->positionals = {{"api_key", "ID of the API key", {"<id>", "<api_key_id>"}}};
    cmd->examples = {
        {"vh api-key info 42", "Show information for the API key with ID 42."},
        {"vh api-key show 42", "Show information for the API key with ID 42 (using alias)."}
    };
    return cmd;
}

std::shared_ptr<CommandUsage> update(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties of an existing API key.";
    cmd->positionals = {{"api_key", "ID of the API key to update", {"<id>", "<api_key_id>"}}};
    cmd->optional = {
        {"name", "New name for the API key", {"name", {"<new_name>"}}},
        {"access_key", "New access key for the S3 provider", {"access", {"<new_access_key>"}}},
        {"secret_key", "New secret key for the S3 provider", {"secret", {"<new_secret_key>"}}},
        {"region", "New region for the S3 provider", {"region", {"<new_region>"}}},
        {"endpoint", "New custom endpoint URL for the S3 provider", {"endpoint", {"<new_endpoint>"}}},
        {"provider", "New S3 provider (" + usage_provider() + ")", {"provider", {"<new_provider>"}}}
    };
    cmd->examples = {
        {"vh api-key update 42 --name newname --region us-east-1", "Update the name and region of the API key with ID 42."},
        {"vh api-key set 42 --secret newsecretkey", "Update the secret key of the API key with ID 42 (using alias)."}
    };
    return cmd;
}

std::shared_ptr<CommandUsage> base(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"api-key", "aku", "ak"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Manage a single API key.";
    cmd->subcommands = {
        list(cmd->weak_from_this()),
        create(cmd->weak_from_this()),
        remove(cmd->weak_from_this()),
        info(cmd->weak_from_this()),
        update(cmd->weak_from_this())
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "API Key Commands";
    book->root = base(parent);
    return book;
}

}
