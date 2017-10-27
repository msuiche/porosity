/*++

Copyright (c) 2017, Matthieu Suiche

Module Name:
    Statement.h

Abstract:
    Porosity.

Author:
    Matthieu Suiche (m) Feb-2017

Revision History:

--*/
#ifndef _STATEMENT_H_
#define _STATEMENT_H_

using namespace std;
using namespace dev;
using namespace dev::eth;

class Statement {
public:
    Statement() {

    }

    ConditionAttribute
    NegateCondition() {

        switch (m_cond) {
            case ConditionEqual:
                m_cond = ConditionDifferent;
                break;
            case ConditionDifferent:
                m_cond = ConditionEqual;
                break;
            case ConditionIsZero:
                m_cond = ConditionIsNotZero;
                break;
            case ConditionIsNotZero:
                m_cond = ConditionIsZero;
                break;
            case ConditionLessThan:
                m_cond = ConditionGreaterEqualThan;
                break;
            case ConditionGreaterEqualThan:
                m_cond = ConditionLessThan;
                break;
            case ConditionGreaterThan:
                m_cond = ConditionLessEqualThan;
                break;
            case ConditionLessEqualThan:
                m_cond = ConditionGreaterThan;
                break;
            case ConditionSignedLessThan:
                m_cond = ConditionSignedGreaterEqualThan;
                break;
            case ConditionSignedGreaterEqualThan:
                m_cond = ConditionSignedLessThan;
                break;
            case ConditionSignedGreaterThan:
                m_cond = ConditionSignedGreatedLessEqualThan;
                break;
            case ConditionSignedGreatedLessEqualThan:
                m_cond = ConditionSignedGreaterThan;
                break;
            default:
                printf("%s: unknown condition.\n", __FUNCTION__);
                //m_cond = ConditionUndefined;
                break;
        }

        return m_cond;
    }


    Statement(StatementName s) {
        m_stmt = s;
    }

    void
    setCondition(InstructionState _instrState) {

        switch (_instrState.offInfo.inst) {
                case Instruction::LT:
                    m_cond = ConditionLessThan;
                    if (_instrState.stack.size()) m_op1 = _instrState.stack[0];
                    if (_instrState.stack.size() > 1) m_op2 = _instrState.stack[1];
                    break;
                case Instruction::GT:
                    m_cond = ConditionGreaterThan;
                    if (_instrState.stack.size()) m_op1 = _instrState.stack[0];
                    if (_instrState.stack.size() > 1) m_op2 = _instrState.stack[1];
                    break;
                case Instruction::SLT:
                    m_cond = ConditionSignedLessThan;
                    if (_instrState.stack.size()) m_op1 = _instrState.stack[0];
                    if (_instrState.stack.size() > 1) m_op2 = _instrState.stack[1];
                    break;
                case Instruction::SGT:
                    m_cond = ConditionSignedGreaterThan;
                    if (_instrState.stack.size()) m_op1 = _instrState.stack[0];
                    if (_instrState.stack.size() > 1) m_op2 = _instrState.stack[1];
                    break;
                case Instruction::EQ:
                    m_cond = ConditionEqual;
                    if (_instrState.stack.size()) m_op1 = _instrState.stack[0];
                    if (_instrState.stack.size() > 1) m_op2 = _instrState.stack[1];
                    break;
                case Instruction::ISZERO:
                    if (m_cond == ConditionUndefined) {
                        if (_instrState.stack.size()) m_op1 = _instrState.stack[0];
                        m_cond = ConditionIsZero;
                    }
                    else NegateCondition();
                    // m_op1 = _instrState.stack[0];
                    break;
                default:
                    //printf("%s: unknown condition.\n", __FUNCTION__);
                    //cond = ConditionUndefined;
                    return;
                break;
        }

    }

    string
    getStatementStr() {
        string result = "";

        if (m_stmt == StatementIf) {
            const char *operators[] = { "", "==", "!=", "!", "", "<", ">=", ">", "<=", 0 };
            result = "if";

            result += " (";
            switch (m_cond) {
            case ConditionDifferent:
            case ConditionEqual:
            case ConditionLessThan:
            case ConditionLessEqualThan:
            case ConditionGreaterThan:
            case ConditionGreaterEqualThan:
                result += InstructionContext::getDismangledRegisterName(&m_op1);
                result += " " + string(operators[m_cond]) + " ";
                result += InstructionContext::getDismangledRegisterName(&m_op2);
                break;
            case ConditionIsZero:
            case ConditionIsNotZero:
                result += string(operators[m_cond]) + m_op1.exp;
            break;
                    
            default:
                break;
            }

            result += ")";
        }
        return result;
    }

    void
    print() {
        printf("%s: %s\n", __FUNCTION__, getStatementStr().c_str());
    }

    ConditionAttribute
    getCondition() { return m_cond; }

    void
    setBlocks(BasicBlockInfo *trueBlock, BasicBlockInfo *falseBlock) {
        m_trueBlock = trueBlock;
        m_falseBlock = falseBlock;
    }

    void setValid() { m_isValid = true; }
    bool isValid() { return m_isValid; }

    StackRegister m_op1;
    StackRegister m_op2;

private:
    StatementName m_stmt = StatementUndefined;
    ConditionAttribute m_cond = ConditionUndefined;
    bool m_isValid = false;

    BasicBlockInfo *m_trueBlock;
    BasicBlockInfo *m_falseBlock;
};

class SourceCode {
public:
    typedef struct SrcLine {
        uint32_t line_number;
        uint32_t depth;
        string line;
        uint32_t errorCode;
    } SrcLine;

    void
    append(
        uint32_t _depth,
        string _line
    ) {
        SrcLine entry{ m_lineCount++, _depth, _line, DCode_OK };
        m_lines.insert(m_lines.end(), entry);
    }

    void
    setErrCode(
        uint32_t _errCode
    ) {
        if (m_lineCount) m_lines[m_lineCount - 1].errorCode = _errCode;
    }

    void
    print() {
        for (auto line = m_lines.begin(); line != m_lines.end(); ++line) {
            DEPTH(line->depth);

            if (!line->errorCode) printf("%s\n", line->line.c_str());
            else if (line->errorCode & DCode_Err) {
                Red("%s", line->line.c_str());
                printf("\n");
            }
            else printf("%s [%d]\n", line->line.c_str(), line->errorCode);
        }

        printf("\n\n");
        getErrors();
    }

    void
    getErrors() {
        for (auto line = m_lines.begin(); line != m_lines.end(); ++line) {
            if (line->errorCode & DCode_Err) {
                Red("L%d (D%d):", line->line_number + 1, line->errorCode);
                printf(" ");
                switch (line->errorCode) {
                    case DCode_Err_ReentrantVulnerablity:
                        printf("Potential reentrant vulnerability found.\n");
                    break;
                }
                printf("\n");
            }
        }
    }


    uint32_t loc() { return m_lines.size(); }

    uint32_t m_lineCount = 0;
    vector <SrcLine> m_lines;
};
#endif
