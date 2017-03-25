# porosity

## Getting Started

Platform         | Status
-----------------|-----------
Windows          | [![Build Status](https://comae.visualstudio.com/_apis/public/build/definitions/13b58962-60a0-48ed-879d-f56575385e2e/4/badge)](https://comae.visualstudio.com/_apis/public/build/definitions/13b58962-60a0-48ed-879d-f56575385e2e/4/badge)

## Overview
Ethereum is gaining a significant popularity in the blockchain community, mainly due to fact that it is design in a way that enables developers to write decentralized applications (Dapps) and smart-contract using blockchain technology.


Ethereum blockchain is a consensus-based globally executed virtual machine, also referred as  Ethereum Virtual Machine (EVM) by implemented its own micro-kernel supporting a handful number of instructions, its own stack, memory and storage. This enables the radical new concept of distributed applications.


Contracts live on the blockchain in an Ethereum-specific binary format (EVM bytecode). However, contracts are typically written in some high-level language such as Solidity and then compiled into byte code to be uploaded on the blockchain. Solidity is a contract-oriented, high-level language whose syntax is similar to that of JavaScript.


This new paradigm of applications opens the door to many possibilities and opportunities. Blockchain is often referred as secure by design, but now that blockchains can embed applications this raise multiple questions regarding architecture, design, attack vectors and patch deployments.


As we, reverse engineers, know having access to source code is often a luxury. Hence, the need for an open-source tool like Porosity: decompiler for EVM bytecode into readable Solidity-syntax contracts – to enable static and dynamic analysis of compiled contracts but also vulnerability discovery.
