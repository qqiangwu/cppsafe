//=- LifetimePset.h - Diagnose lifetime violations -*- C++ -*-================//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H

#include "cppsafe/lifetime/LifetimeAttrData.h"
#include "cppsafe/lifetime/LifetimeTypeCategory.h"

#include "clang/AST/Decl.h"
#include "clang/AST/ExprCXX.h"
#include <llvm/ADT/StringExtras.h>
#include <range/v3/algorithm/find_if_not.hpp>
#include <range/v3/view/reverse.hpp>

#include <map>
#include <set>

namespace clang::lifetime {

/// A Variable can represent a base:
/// - a local variable: Var contains a non-null VarDecl
/// - the this pointer: Var contains a non-null RecordDecl
/// - temporary: Var contains a non-null MaterializeTemporaryExpr
/// - the return value of the current function: Var contains a null Expr
/// plus fields of them (in member FDs).
/// And a list of dereference and field select operations that applied
/// consecutively to the base.
class Variable : public ContractVariable {
public:
    Variable(const VarDecl* VD)
        : ContractVariable(VD)
    {
    }
    Variable(const MaterializeTemporaryExpr* MT)
        : ContractVariable(MT)
    {
        assert(MT);
    }

    Variable(const ContractVariable& CV, const FunctionDecl* FD)
        : ContractVariable(CV)
    {
        if (asParmVarDecl()) {
            Var = FD->getParamDecl(asParmVarDecl()->getFunctionScopeIndex());
        }
    }

    static Variable thisPointer(const RecordDecl* RD) { return Variable(RD); }

    /// A variable that represent the return value of the current function.
    static Variable returnVal() { return Variable(ContractVariable::returnVal(), nullptr); }

    // Is O a subobject of this?
    // Examples:
    //   a is the subobject of a
    //   *a is the subobject of *a
    //   **a is the subobject of **a
    //   a.b.c is the subobject of a.b
    //   (*a).b is the subobject of *a
    //   *(*a).b is NOT the subobject of *a
    bool isParent(const Variable& O) const
    {
        auto isPrefixOf = [this](const llvm::SmallVectorImpl<const FieldDecl*>& OtherFDs) {
            if (OtherFDs.size() < FDs.size()) {
                return false;
            }
            bool HasField = false;
            for (const auto* FD : OtherFDs) {
                if (FD) {
                    HasField = true;
                }
                // Dereferencing a field, we are no longer in the same object.
                if (HasField && !FD) {
                    return false;
                }
            }
            return FDs.end() == std::mismatch(FDs.begin(), FDs.end(), OtherFDs.begin()).first;
        };
        return Var == O.Var && isPrefixOf(O.FDs);
    }

    bool hasStaticLifetime() const
    {
        if (const auto* VD = Var.dyn_cast<const VarDecl*>()) {
            return VD->hasGlobalStorage();
        }
        return isThisPointer() && !FDs.empty() && llvm::none_of(FDs, [](const FieldDecl* FD) { return FD == nullptr; });
    }

    /// Returns QualType of Variable.
    /// \pre !isReturnVal()
    /// TODO: Should we cache the type instead of calculating?
    QualType getType() const
    {
        int Order;
        return getTypeAndOrder(Order);
    }

    // Return the type of the base object of this variable, ignoring all
    // fields and derefs.
    // \post never returns a null QualType
    QualType getBaseType() const
    {
        assert(!isReturnVal() && "We don't store types of return values here");
        if (const auto* VD = asVarDecl()) {
            return VD->getType();
        } else if (const MaterializeTemporaryExpr* MT = asTemporary()) {
            return MT->getType();
        } else if (const RecordDecl* RD = asThis()) {
            return RD->getASTContext().getPointerType(RD->getASTContext().getRecordType(RD));
        } else {
            llvm_unreachable("invalid state");
        }
    }

    bool isField() const { return !FDs.empty() && FDs.back(); }

    bool isThisPointer() const { return asThis(); }

    bool isTemporary() const { return asTemporary(); }

    /// When VD is non-null, returns true if the Variable represents a
    /// lifetime-extended temporary that is extended by VD. When VD is null,
    /// returns true if the the Variable is a non-lifetime-extended temporary.
    bool isTemporaryExtendedBy(const ValueDecl* VD) const
    {
        return asTemporary() && asTemporary()->getExtendingDecl() == VD;
    }

    const VarDecl* asVarDecl() const { return Var.dyn_cast<const VarDecl*>(); }

    // Chain of field accesses starting from VD. Types must match.
    void addFieldRef(const FieldDecl* FD)
    {
        assert(FD);

#ifndef NDEBUG
        // We can only add fields that are part of the current record.
        QualType QT = getType();
        // Fields can only be added if the current type is a record
        assert(!QT.isNull());
        const CXXRecordDecl* RD = QT->getAsCXXRecordDecl();
        assert(RD);

        // Either the fields is a field of this class, or of a base class
        // or of a derived class (in case of static up-cast).
        assert(FD->getParent() == RD || RD->isDerivedFrom(dyn_cast<CXXRecordDecl>(FD->getParent()))
            || dyn_cast<CXXRecordDecl>(FD->getParent())->isDerivedFrom(RD));
#endif
        FDs.push_back(FD);
    }

    Variable& deref(int Num = 1)
    {
        while (Num--) {
            FDs.push_back(nullptr);
        }
        return *this;
    }

    bool isDeref() const { return !FDs.empty() && FDs.front() == nullptr; }

    unsigned getOrder() const
    {
        int Order;
        getTypeAndOrder(Order);
        return Order >= 0 ? Order : 0;
    }

    unsigned getDerefNum() const
    {
        const auto Rev = ranges::views::reverse(FDs);
        const auto It = ranges::find_if_not(Rev, [](const auto* F) { return F == nullptr; });
        return It - Rev.begin();
    }

    std::string getName() const
    {
        std::string Ret;
        if (const MaterializeTemporaryExpr* MTE = asTemporary()) {
            if (MTE->getExtendingDecl()) {
                Ret = "(lifetime-extended temporary through " + MTE->getExtendingDecl()->getName().str() + ")";
            } else {
                Ret = "(temporary)";
            }

        } else if (auto* VD = asVarDecl()) {
            Ret = VD->getName().str();
        } else if (isThisPointer()) {
            Ret = "this";
        } else if (isReturnVal()) {
            Ret = "(return value)";
        } else {
            llvm_unreachable("Invalid state");
        }

        for (unsigned I = 0; I < FDs.size(); ++I) {
            if (FDs[I]) {
                if (I > 0 && !FDs[I - 1]) {
                    Ret = "(" + Ret + ")";
                }
                Ret += "." + std::string(FDs[I]->getName());
            } else {
                Ret.insert(0, 1, '*');
            }
        }
        return Ret;
    }

private:
    // The this pointer
    Variable(const RecordDecl* RD)
        : ContractVariable(RD)
    {
    }

    QualType getTypeAndOrder(int& Order) const
    {
        Order = -1;
        QualType Base = getBaseType();

        if (classifyTypeCategory(Base) == TypeCategory::Owner) {
            Order = 0;
        }

        for (const auto* FD : FDs) {
            if (FD) {
                Base = FD->getType();
                if (Order == -1 && classifyTypeCategory(Base) == TypeCategory::Owner) {
                    Order = 0;
                }
            } else {
                // Dereference the current pointer/owner
                // consider: ****v
                if (!Base.isNull()) {
                    Base = getPointeeType(Base);
                }

                if (Order >= 0) {
                    ++Order;
                }
            }
        }
        return Base;
    }

    const MaterializeTemporaryExpr* asTemporary() const
    {
        return dyn_cast_or_null<MaterializeTemporaryExpr>(Var.dyn_cast<const Expr*>());
    }
};

/// The reason why a pset became invalid
/// Invariant: (Reason != POINTEE_LEFT_SCOPE || Pointee) && Range.isValid()
class InvalidationReason {
    NoteType Reason;
    const VarDecl* Pointee;
    SourceRange Range;
    const CFGBlock* Block;
    std::optional<Variable> InvalidatedMemory;

    InvalidationReason(SourceRange Range, const CFGBlock* Block, NoteType Reason, const VarDecl* Pointee = nullptr)
        : Reason(Reason)
        , Pointee(Pointee)
        , Range(Range)
        , Block(Block)
    {
        assert(Range.isValid());
    }

public:
    SourceRange getRange() const { return Range; }
    std::optional<Variable> getInvalidatedMemory() const { return InvalidatedMemory; }
    void setInvalidatedMemory(const Variable& V) { InvalidatedMemory = V; }
    const CFGBlock* getBlock() const { return Block; }

    void emitNote(LifetimeReporterBase& Reporter) const
    {
        if (Reason == NoteType::PointeeLeftScope) {
            assert(Pointee);
            Reporter.notePointeeLeftScope(Range, Pointee->getNameAsString());
            return;
        }
        Reporter.note(Reason, Range);
    }

    static InvalidationReason NotInitialized(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::NeverInit };
    }

    static InvalidationReason PointeeLeftScope(SourceRange Range, const CFGBlock* Block, const VarDecl* Pointee)
    {
        assert(Pointee);
        return { Range, Block, NoteType::PointeeLeftScope, Pointee };
    }

    static InvalidationReason TemporaryLeftScope(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::TempDestroyed };
    }

    static InvalidationReason Dereferenced(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::Dereferenced };
    }

    static InvalidationReason ForbiddenCast(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::ForbiddenCast };
    }

    static InvalidationReason Modified(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::Modified };
    }

    static InvalidationReason Deleted(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::Deleted };
    }

    static InvalidationReason Moved(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::Moved };
    }
};

/// The reason how null entered a pset.
class NullReason {
    SourceRange Range;
    const CFGBlock* Block;
    NoteType Reason;
    std::optional<Variable> NulledMemory;

public:
    std::optional<Variable> getNulledMemory() const { return NulledMemory; }
    void setNulledMemory(const Variable& V) { NulledMemory = V; }
    const CFGBlock* getBlock() const { return Block; }

    NullReason(SourceRange Range, const CFGBlock* Block, NoteType Reason)
        : Range(Range)
        , Block(Block)
        , Reason(Reason)
    {
        assert(Range.isValid());
    }

    static NullReason assigned(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::Assigned };
    }

    static NullReason parameterNull(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::ParamNull };
    }

    static NullReason defaultConstructed(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::NullDefaultConstructed };
    }

    static NullReason comparedToNull(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::ComparedToNull };
    }

    static NullReason nullptrConstant(SourceRange Range, const CFGBlock* Block)
    {
        return { Range, Block, NoteType::NullConstant };
    }

    void emitNote(LifetimeReporterBase& Reporter) const
    {
        if (Reason == NoteType::NullConstant) {
            return; // not diagnosed, hopefully obvious
        }
        Reporter.note(Reason, Range);
    }
};

/// A pset (points-to set) can contain:
/// - null
/// - static
/// - invalid
/// - variables
/// It a Pset contains non of that, its "unknown".
class PSet {
public:
    // Initializes an unknown pset
    PSet()
        : ContainsNull(false)
        , ContainsInvalid(false)
        , ContainsGlobal(false)
    {
    }
    PSet(const ContractPSet& S, const FunctionDecl* FD)
        : ContainsNull(S.ContainsNull)
        , ContainsInvalid(S.ContainsInvalid)
        , ContainsGlobal(S.ContainsGlobal)
    {
        for (const ContractVariable& CV : S.Vars) {
            assert(CV != ContractVariable::returnVal());
            Vars.emplace(CV, FD);
        }
    }

    bool operator==(const PSet& O) const
    {
        return ContainsInvalid == O.ContainsInvalid && ContainsNull == O.ContainsNull
            && ContainsGlobal == O.ContainsGlobal && Vars == O.Vars;
    }

    void explainWhyInvalid(LifetimeReporterBase& Reporter) const
    {
        for (auto& R : InvReasons) {
            R.emitNote(Reporter);
        }
    }

    void explainWhyNull(LifetimeReporterBase& Reporter) const
    {
        for (auto& R : NullReasons) {
            R.emitNote(Reporter);
        }
    }

    bool shouldBeFilteredBasedOnNotes(LifetimeReporterBase& Reporter) const
    {
        if (!Reporter.shouldFilterWarnings()) {
            return false;
        }
        for (auto& R : InvReasons) {
            if (auto V = R.getInvalidatedMemory()) {
                if (Reporter.shouldBeFiltered(R.getBlock(), &V.value())) {
                    return true;
                }
            }
        }
        for (auto& R : NullReasons) {
            if (auto V = R.getNulledMemory()) {
                if (Reporter.shouldBeFiltered(R.getBlock(), &V.value())) {
                    return true;
                }
            }
        }
        return false;
    }

    bool containsInvalid() const { return ContainsInvalid; }
    bool isInvalid() const { return !ContainsNull && !ContainsGlobal && ContainsInvalid && Vars.empty(); }

    bool isUnknown() const { return !ContainsInvalid && !ContainsNull && !ContainsGlobal && Vars.empty(); }

    /// Returns true if we look for S and we have S.field in the set.
    bool containsParent(Variable Var) const
    {
        return llvm::any_of(Vars, [Var](const Variable& Other) { return Var.isParent(Other); });
    }

    bool containsNull() const { return ContainsNull; }
    bool isNull() const { return ContainsNull && !ContainsGlobal && !ContainsInvalid && Vars.empty(); }
    void addNull(NullReason Reason)
    {
        if (ContainsNull) {
            return;
        }
        ContainsNull = true;
        NullReasons.push_back(Reason);
    }
    void removeNull()
    {
        ContainsNull = false;
        NullReasons.clear();
    }
    void removeEverythingButNull()
    {
        ContainsInvalid = false;
        InvReasons.clear();
        ContainsGlobal = false;
        Vars.clear();
    }

    void addNullReason(NullReason Reason)
    {
        assert(ContainsNull);
        NullReasons.push_back(Reason);
    }

    bool containsGlobal() const { return ContainsGlobal; }
    bool isGlobal() const { return ContainsGlobal && !ContainsNull && !ContainsInvalid && Vars.empty(); }
    void addGlobal() { ContainsGlobal = true; }

    const std::set<Variable>& vars() const { return Vars; }

    const std::vector<InvalidationReason>& invReasons() const { return InvReasons; }
    const std::vector<NullReason>& nullReasons() const { return NullReasons; }

    void addReasonTarget(const Variable& V)
    {
        for (auto& R : InvReasons) {
            R.setInvalidatedMemory(V);
        }
        for (auto& R : NullReasons) {
            R.setNulledMemory(V);
        }
    }

    bool checkSubstitutableFor(const PSet& O, SourceRange Range, LifetimeReporterBase& Reporter,
        ValueSource Source = ValueSource::Param, StringRef SourceName = "")
    {
        // Everything is substitutable for invalid.
        if (O.ContainsInvalid) {
            return true;
        }

        // If 'this' includes invalid, then 'O' must include invalid.
        if (ContainsInvalid) {
            Reporter.warnNullDangling(WarnType::Dangling, Range, Source, SourceName, !isInvalid());
            explainWhyInvalid(Reporter);
            return false;
        }

        // If 'this' includes null, then 'O' must include null.
        if (ContainsNull && !O.ContainsNull) {
            Reporter.warnNullDangling(WarnType::Null, Range, Source, SourceName, !isNull());
            explainWhyNull(Reporter);
            return false;
        }

        // If 'O' includes static and no x or o, then 'this' must include static and
        // no x or o.
        if (O.ContainsGlobal) {
            // here: 'this' is not Invalid, if 'this' is null, O must contains null, checked before
            if (!ContainsGlobal && !Vars.empty()) {
                Reporter.warnWrongPset(Range, Source, SourceName, str(), O.str());
                return false;
            }
        }

        // If 'this' includes o'', then 'O' must include o'' or o'. (etc.)
        for (const auto& V : Vars) {
            auto I = O.Vars.find(V);
            if (I == O.Vars.end() || I->getOrder() > V.getOrder()) {
                Reporter.warnWrongPset(Range, Source, SourceName, str(), O.str());
                return false;
            }
        }

        // TODO
        // If 'this' includes o'', then 'O' must include o'' or o'. (etc.)
        // If 'this' includes o', then 'O' must include o'.
        return true;
    }

    std::string str() const
    {
        if (isUnknown()) {
            return "((unknown))";
        }
        SmallVector<std::string, 16> Entries;
        if (ContainsInvalid) {
            Entries.push_back("(invalid)");
        }
        if (ContainsNull) {
            Entries.push_back("(null)");
        }
        if (ContainsGlobal) {
            Entries.push_back("(global)");
        }
        for (const auto& V : Vars) {
            Entries.push_back(V.getName());
        }
        std::sort(Entries.begin(), Entries.end());
        return "(" + llvm::join(Entries, ", ") + ")";
    }

    void print(raw_ostream& Out) const { Out << str() << "\n"; }

    /// Merge contents of other pset into this.
    void merge(const PSet& O)
    {
        if (!ContainsInvalid && O.ContainsInvalid) {
            ContainsInvalid = true;
            InvReasons = O.InvReasons;
        }

        if (!ContainsNull && O.ContainsNull) {
            ContainsNull = true;
            NullReasons = O.NullReasons;
        }
        ContainsGlobal |= O.ContainsGlobal;

        Vars.insert(O.Vars.begin(), O.Vars.end());
    }

    // This method is used to actualize the PSet of a contract with the arguments
    // of a call. It has two modes:
    // * Checking: The special elements of the psets are coming from the
    //   contracts, i.e.: the resulting pset will contain null only if the
    //   contract had null.
    // * Non-checking: The resulting pset will contain null if 'To' contains
    //   null. This is useful so code like 'int *p = f(&x);' will result in a
    //   non-null pset for 'p'.
    void bind(Variable ToReplace, const PSet& To, bool Checking = true)
    {
        // Replace valid deref locations.
        if (Vars.erase(ToReplace)) {
            if (Checking) {
                Vars.insert(To.Vars.begin(), To.Vars.end());
            } else {
                merge(To); // TODO: verify if assigned here note is generated later on
                           // during output matching.
            }
        }
    }

    PSet operator+(const PSet& O) const
    {
        PSet Ret = *this;
        Ret.merge(O);
        return Ret;
    }

    void insert(Variable Var, unsigned Deref = 0)
    {
        if (Var.hasStaticLifetime()) {
            ContainsGlobal = true;
            return;
        }

        Vars.insert(Var);
    }

    void addFieldRefIfTypeMatch(const FieldDecl* FD)
    {
        std::set<Variable> NewVars;
        for (auto Var : Vars) {
            if (const auto Ty = Var.getType(); !Ty.isNull()) {
                if (const auto* RD = Ty->getAsCXXRecordDecl()) {
                    if (FD->getParent() == RD
                        || (RD->hasDefinition() && RD->isDerivedFrom(dyn_cast<CXXRecordDecl>(FD->getParent())))
                        || dyn_cast<CXXRecordDecl>(FD->getParent())->isDerivedFrom(RD)) {
                        Var.addFieldRef(FD);
                    }
                }
            }

            NewVars.insert(Var);
        }
        Vars = NewVars;
    }

    /// The pointer is dangling
    static PSet invalid(InvalidationReason Reason) { return invalid(std::vector<InvalidationReason> { Reason }); }

    /// The pointer is dangling
    static PSet invalid(const std::vector<InvalidationReason>& Reasons)
    {
        PSet ret;
        ret.ContainsInvalid = true;
        ret.InvReasons = Reasons;
        return ret;
    }

    /// A pset that contains only (null)
    static PSet null(NullReason Reason)
    {
        PSet ret;
        ret.ContainsNull = true;
        ret.NullReasons.push_back(Reason);
        return ret;
    }

    /// A pset that contains (global), (null)
    static PSet globalVar(bool Nullable = false)
    {
        PSet ret;
        ret.ContainsNull = Nullable;
        ret.ContainsGlobal = true;
        return ret;
    }

    /// The pset contains one element
    static PSet singleton(Variable Var, unsigned Deref = 0)
    {
        PSet ret;
        if (Var.hasStaticLifetime()) {
            ret.ContainsGlobal = true;
        } else {
            Var.deref(Deref);
            ret.Vars.emplace(Var);
        }
        return ret;
    }

private:
    unsigned ContainsNull : 1;
    unsigned ContainsInvalid : 1;
    unsigned ContainsGlobal : 1;
    std::set<Variable> Vars;

    std::vector<InvalidationReason> InvReasons;
    std::vector<NullReason> NullReasons;
}; // namespace lifetime

using PSetsMap = std::map<Variable, PSet>;

} // namespace clang::lifetime

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEPSET_H
