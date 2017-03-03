#include "Porosity.h"

uint32_t g_VerboseLevel = VERBOSE_LEVEL;
bytes defaultArguments = { 0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

void header() {
    printf("    Porosity. v0.0.1 (Feb 2017)\n");
    printf("    A decompiler for blockchain-based smart contract bytecode.\n");
    printf("    Matt Suiche - m@comae.io\n");
    printf("\n");
}

typedef enum _Method {
    MethodNone = 0x0,
    MethodListFunctions = (1 << 0),
    MethodSingleStep = (1 << 1),
    MethodDecompile = (1 << 2),
    MethodDisassm = (1 << 3),
    MethodSingleStepping = (1 << 4),
    MethodControlFlowGraph = (1 << 5)
} Method;

typedef struct _Arguments {
    int verboseLevel;
    bytes codeByte;
    bytes codeByteRuntime;
    bytes arguments;

    string abiMethod;
    string abiMethodFile;

    uint32_t targetHashMethod;
    uint32_t method;

    bool debugMode;
} Arguments;

bool
parse(
    int argc,
    char **argv,
    Arguments *out
)
{
    out->verboseLevel = VERBOSE_LEVEL;

    for (int i = 0; i < argc; i++) {
        string keyword = argv[i];
        if ((keyword == "--verbose") && ((i + 1) < argc)) {
            out->verboseLevel = atoi(argv[++i]);
        }
        else if ((keyword == "--debug")) {
            out->debugMode = true;
        }
        else if ((keyword == "--abi") && ((i + 1) < argc)) {
            out->abiMethod = argv[++i];
        }
        else if ((keyword == "--abi-file") && ((i + 1) < argc)) {
            out->abiMethodFile = argv[++i];
        }
        else if ((keyword == "--hash") && ((i + 1) < argc)) {
            sscanf_s(argv[i], "%x", &out->targetHashMethod);
        }
        else if ((keyword == "--list")) {
            out->method |= MethodListFunctions;
        }
        else if ((keyword == "--runtime-code") && ((i + 1) < argc)) {
            out->codeByteRuntime = fromHex(argv[++i]);
        }
        else if ((keyword == "--code") && ((i + 1) < argc)) {
            out->codeByte = fromHex(argv[++i]);
        }
        else if ((keyword == "--disassm")) {
            out->method |= MethodDisassm;
        }
        else if ((keyword == "--decompile")) {
            out->method |= MethodDecompile;
        }
        else if ((keyword == "--single-step")) {
            out->method |= MethodSingleStepping;
        }
        else if ((keyword == "--cfg")) {
            out->method |= MethodControlFlowGraph;
        }
        else if ((keyword == "--arguments") && ((i + 1) < argc)) {
            out->arguments = fromHex(argv[++i]);
        }
    }

    g_VerboseLevel = out->verboseLevel;

    //
    // Validate parameters.
    // Parameter initialization
    //
    if (!out->debugMode) {
        if (out->codeByteRuntime.empty()) {
            printf("%s: Please at least provide some byte code (--runtime-code) or run it in debug mode (--debug) with pre-configured inputs.\n", __FUNCTION__);
            return false;
        }

        if (out->arguments.empty()) {
            out->arguments = defaultArguments;
        }
    }

    return true;
}

void
help() {
    printf("\n");
    printf("Usage:\n");
    printf("    --debug                             - Enable debug mode. (testing only - no input parameter needed.)\n");
    printf("Input parameters:\n");
    printf("    --runtime-code <bytecode>           - Ethereum bytecode. (mandatory)\n");
    printf("    --arguments <arguments>             - Ethereum arguments to pass to the function. (optional, default data set provided if not provided.)\n");
    printf("    --abi <arguments>                   - Ethereum Application Binary Interface (ABI) in JSON format. (optional but recommended)\n");
    printf("    --hash <hashmethod>                 - Work on a specific function, can be retrieved wit --list. (optional)\n");
    printf("\n");
    printf("Features:\n");
    printf("    --list                              - List identified methods/functions.\n");
    printf("    --disassm                           - Disassemble the bytecode.\n");
    printf("    --single-step                       - Execute the byte code through our VM.\n");
    printf("    --cfg                               - Generate a the control flow graph in Graphviz format.\n");
    printf("    --decompile                         - Decompile a given function or all the bytecode.\n");

    printf("\n");
    return;
}

int main(
    int argc,
    char **argv
)
{
    Arguments args = { 0 };
    header();

    if (!parse(argc, argv, &args)) {
        help();
        return false;
    }

    if (args.debugMode) {
        debug();
        return true;
    }

    Contract contract(args.codeByteRuntime);
    contract.setABI(args.abiMethodFile, args.abiMethod);
    contract.setData(args.arguments);

    if (args.method & MethodListFunctions) {
        contract.printFunctions();
    }
    else if (args.method & MethodDisassm) {
        contract.printInstructions();
    }
    else if (args.method & MethodDecompile) {
        contract.forEachFunction([&](uint32_t _hash) {
            if (!args.targetHashMethod || (_hash == args.targetHashMethod)) {
                printf("Hash: 0x%08X\n", _hash);
                contract.getFunction(_hash);
            }
        });
    }
    else if (args.method & MethodControlFlowGraph) {
        printf("%s\n", contract.getGraphviz().c_str());
    }

    return true;
}