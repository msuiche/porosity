/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Porosity.cpp

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#include "Porosity.h"

#define SUCCESS 0
#define FAILED 1

uint32_t g_VerboseLevel = VERBOSE_LEVEL;
bool g_SingleStepping = false;
bytes defaultArguments = { 
    0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // arg1
    0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // arg2
    0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // arg3
    0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // arg4
    0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // arg5
    0xee, 0xe9, 0x72, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // arg6
    0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

void header() {
    printf("Porosity v0.1 (https://www.comae.io)\n");
    printf("Matt Suiche, Comae Technologies <support@comae.io>\n");
    printf("The Ethereum bytecode commandline decompiler.\n");
    printf("Decompiles the given Ethereum input bytecode and outputs the Solidity code.\n");
    printf("\n");
}

typedef enum _Method {
    MethodNone = 0x0,
    MethodListFunctions = (1 << 0),
    MethodSingleStep = (1 << 1),
    MethodDecompile = (1 << 2),
    MethodDisassm = (1 << 3),
    MethodSingleStepping = (1 << 4),
    MethodControlFlowGraph = (1 << 5),
    MethodControlFlowGraphFull = (1 << 6),
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
    bool noHeader;
} Arguments;

bool
parse(
    int argc,
    char **argv,
    Arguments *out
)
{
    out->verboseLevel = VERBOSE_LEVEL;
    out->noHeader = false;
    for (int i = 1; i < argc; i++) {
        string kw;
        string arg;
        bool inArg = false;
        for (char *c = argv[i]; *c; ++c) {
            if (!inArg) {
                if (*c == '=') {
                    inArg = true;
                    continue;
                }
                kw += *c;
            } else {
                arg += *c;
            }
        }
        if ((i + 1) < argc) {
            char *next = argv[i+1];
            if (*next != '-') {
                // The next argument is the arg. Save it here if arg isn't
                // already set with '=', and skip it in the loop.
                if (arg.size() == 0) {
                    arg = next;
                }
                i++;
            }
        }
        if ((kw == "--verbose") && arg.size()) {
            out->verboseLevel = atoi(arg.c_str());
        }
        else if ((kw == "--debug")) {
            out->debugMode = true;
        }
        else if ((kw == "--abi") && arg.size()) {
            out->abiMethod = arg;
        }
        else if ((kw == "--abi-file") && arg.size()) {
            out->abiMethodFile = arg;
        }
        else if ((kw == "--hash") && arg.size()) {
            dev::FixedHash<4> hash(fromHex(arg));
            out->targetHashMethod = (hash[0] << 24) | (hash[1] << 16) | (hash[2] << 8) | hash[3];
            printf("Target function: 0x%08x\n", out->targetHashMethod);
        }
        else if ((kw == "--list")) {
            out->method |= MethodListFunctions;
        }
        else if (((kw == "--runtime-code") || (kw == "--code")) && arg.size()) {
            out->codeByteRuntime = fromHex(arg);
        }
        else if ((kw == "--code") && arg.size()) {
            out->codeByte = fromHex(arg);
        }
        else if ((kw == "--code-file") && arg.size()) {
            string str;

            ifstream file(arg);
            if (file.good()) {
                file.seekg(0, std::ios::end);
                str.reserve(file.tellg());
                file.seekg(0, std::ios::beg);

                str.assign((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

                out->codeByte = fromHex(str);
                out->codeByteRuntime = fromHex(str);
            }
        }
        else if ((kw == "--disassm") || (kw == "--disasm")) {
            out->method |= MethodDisassm;
        }
        else if ((kw == "--decompile")) {
            out->method |= MethodDecompile;
        }
        else if ((kw == "--single-step")) {
            out->method |= MethodSingleStepping;
            g_SingleStepping = true;
        }
        else if ((kw == "--cfg")) {
            out->method |= MethodControlFlowGraph;
            out->noHeader = true;
        }
        else if ((kw == "--cfg-full")) {
            out->method |= MethodControlFlowGraphFull;
            out->noHeader = true;
        }
        else if ((kw == "--arguments") && arg.size()) {
            out->arguments = fromHex(arg);
        }
    }

    g_VerboseLevel = out->verboseLevel;

    //
    // Validate parameters.
    // Parameter initialization
    //
    if (!out->debugMode) {
        if (out->codeByteRuntime.empty()) {
            printf("%s: Please at least provide some byte code (--code) or run it in debug mode (--debug) with pre-configured inputs.\n", __FUNCTION__);
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
    printf("Usage: porosity [options]\n");
    printf("Debug:\n");
    printf("    --debug                             - Enable debug mode. (testing only - no input parameter needed.)\n\n");
    printf("Input parameters:\n");
    printf("    --code <bytecode>                   - Ethereum bytecode. (mandatory)\n");
    printf("    --code-file <filename>              - Read ethereum bytecode from file\n");
    printf("    --arguments <arguments>             - Ethereum arguments to pass to the function. (optional, default data set provided if not provided.)\n");
    printf("    --abi <arguments>                   - Ethereum Application Binary Interface (ABI) in JSON format. (optional but recommended)\n");
    printf("    --hash <hashmethod>                 - Work on a specific function, can be retrieved wit --list. (optional)\n");
    printf("\n");
    printf("Features:\n");
    printf("    --list                              - List identified methods/functions.\n");
    printf("    --disassm                           - Disassemble the bytecode.\n");
    printf("    --single-step                       - Execute the byte code through our VM.\n");
    printf("    --cfg                               - Generate a the control flow graph in Graphviz format.\n");
    printf("    --cfg-full                          - Generate a the control flow graph in Graphviz format (including instructions)\n");
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

    if (!parse(argc, argv, &args)) {
        header();
        help();
        return FAILED;
    }


    if (!args.noHeader) header();

    if (args.debugMode) {
        debug();
        return SUCCESS;
    }

    Contract contract(args.codeByteRuntime);
    contract.setABI(args.abiMethodFile, args.abiMethod);
    contract.setData(args.arguments);

    if (args.method & MethodControlFlowGraph) {
        printf("%s\n", contract.getGraphviz(false).c_str());
        return SUCCESS;
    }
    else if (args.method & MethodControlFlowGraphFull) {
        printf("%s\n", contract.getGraphviz(true).c_str());
        return SUCCESS;
    }

    if (args.method & MethodListFunctions) {
        contract.printFunctions();
    }
    else if (args.method & MethodDisassm) {
        contract.printInstructions();
    }
    else if (args.method & MethodDecompile) {
        if (args.targetHashMethod) contract.decompile(args.targetHashMethod);
        else {
            contract.forEachFunction([&](uint32_t _hash) {
                if (!args.targetHashMethod || (_hash == args.targetHashMethod)) {
                    printf("Hash: 0x%08X\n", _hash);
                    // contract.getFunction(_hash);
                    contract.decompile(_hash);
                }
            });
        }
    }

    return SUCCESS;
}
