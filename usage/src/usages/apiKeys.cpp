#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::aku {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::shared_ptr<CommandUsage>& parent) {
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

static std::shared_ptr<CommandUsage> list(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all API keys in the system.";
    cmd->optional = {
        {"--json", "Output the list in JSON format"}
    };
    cmd->examples.push_back({"vh api-keys", "List all API keys."});
    cmd->examples.push_back({"vh api-keys --json", "List all API keys in JSON format."});
    return cmd;
}

std::shared_ptr<CommandUsage> create(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new API key for accessing S3 storage.";
    cmd->required = {
        {"--name <name>", "Name for the new API key"},
        {"--access <accessKey>", "Access key for the S3 provider"},
        {"--secret <secret>", "Secret key for the S3 provider"},
        {"--provider <provider>", "S3 provider (e.g. aws, cloudflare-r2, wasabi, backblaze-b2, digitalocean, minio, ceph, storj, other)"}
    };
    cmd->optional = {
        {"--region <region=auto>", "Region for the S3 provider (default: auto-detect)"},
        {"--endpoint <endpoint>", "Custom endpoint URL for the S3 provider (required for 'other' provider)"}
    };
    cmd->examples.push_back({"vh api-key create --name mykey --access AKIA... --secret wJalrXUtnFEMI/K7MDENG/bPxRfiCYzEXAMPLEKEY --provider aws --region us-west-2",
                           "Create a new AWS API key named 'mykey'."});
    cmd->examples.push_back({"vh api-key new --name r2key --access R2ACCESSKEY --secret R2SECRETKEY --provider cloudflare-r2 --endpoint https://<account_id>.r2.cloudflarestorage.com",
                           "Create a new Cloudflare R2 API key named 'r2key'."});
    return cmd;
}

std::shared_ptr<CommandUsage> remove(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing API key by ID.";
    cmd->positionals = {{"<id>", "ID of the API key to delete"}};
    cmd->examples.push_back({"vh api-key delete 42", "Delete the API key with ID 42."});
    cmd->examples.push_back({"vh api-key rm 42", "Delete the API key with ID 42 (using alias)."});
    return cmd;
}

std::shared_ptr<CommandUsage> info(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about an API key.";
    cmd->positionals = {{"<id>", "ID of the API key"}};
    cmd->examples.push_back({"vh api-key info 42", "Show information for the API key with ID 42."});
    cmd->examples.push_back({"vh api-key show 42", "Show information for the API key with ID 42 (using alias)."});
    return cmd;
}

std::shared_ptr<CommandUsage> update(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties of an existing API key.";
    cmd->positionals = {{"<id>", "ID of the API key to update"}};
    cmd->optional = {
        {"--name <name>", "New name for the API key"},
        {"--access <accessKey>", "New access key for the S3 provider"},
        {"--secret <secret>", "New secret key for the S3 provider"},
        {"--region <region>", "New region for the S3 provider"},
        {"--endpoint <endpoint>", "New custom endpoint URL for the S3 provider"},
        {"--provider <provider>", "New S3 provider (e.g. aws, cloudflare-r2, wasabi, backblaze-b2, digitalocean, minio, ceph, storj, other)"}
    };
    cmd->examples = {
        {"vh api-key update 42 --name newname --region us-east-1", "Update the name and region of the API key with ID 42."},
        {"vh api-key set 42 --secret newsecretkey", "Update the secret key of the API key with ID 42 (using alias)."}
    };
    return cmd;
}

std::shared_ptr<CommandUsage> base(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->description = "Manage a single API key.";
    cmd->subcommands = {
        list(cmd->shared_from_this()),
        create(cmd->shared_from_this()),
        remove(cmd->shared_from_this()),
        info(cmd->shared_from_this()),
        update(cmd->shared_from_this())
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::shared_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "API Key Commands";
    book->root = base(parent);
    return book;
}

}
