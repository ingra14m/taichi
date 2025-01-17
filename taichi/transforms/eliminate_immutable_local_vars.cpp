#include "taichi/ir/ir.h"
#include "taichi/ir/statements.h"
#include "taichi/ir/analysis.h"
#include "taichi/ir/visitors.h"
#include "taichi/system/profiler.h"

namespace taichi::lang {

// The EliminateImmutableLocalVars pass eliminates all immutable local vars
// calculated from the GatherImmutableLocalVars pass. An immutable local var
// can be eliminated by forwarding the value of its only store to all loads
// after that store. See https://github.com/taichi-dev/taichi/pull/6926 for the
// background of this optimization.
class EliminateImmutableLocalVars : public BasicStmtVisitor {
 private:
  using BasicStmtVisitor::visit;

  DelayedIRModifier modifier_;
  std::unordered_set<Stmt *> immutable_local_vars_;
  std::unordered_map<Stmt *, Stmt *> immutable_local_var_to_value_;

 public:
  explicit EliminateImmutableLocalVars(
      const std::unordered_set<Stmt *> &immutable_local_vars)
      : immutable_local_vars_(immutable_local_vars) {
  }

  void visit(AllocaStmt *stmt) override {
    if (immutable_local_vars_.find(stmt) != immutable_local_vars_.end()) {
      modifier_.erase(stmt);
    }
  }

  void visit(LocalLoadStmt *stmt) override {
    if (immutable_local_vars_.find(stmt->src) != immutable_local_vars_.end()) {
      stmt->replace_usages_with(immutable_local_var_to_value_[stmt->src]);
      modifier_.erase(stmt);
    }
  }

  void visit(LocalStoreStmt *stmt) override {
    if (immutable_local_vars_.find(stmt->dest) != immutable_local_vars_.end()) {
      TI_ASSERT(immutable_local_var_to_value_.find(stmt->dest) ==
                immutable_local_var_to_value_.end());
      immutable_local_var_to_value_[stmt->dest] = stmt->val;
      modifier_.erase(stmt);
    }
  }

  static void run(IRNode *node) {
    EliminateImmutableLocalVars pass(
        irpass::analysis::gather_immutable_local_vars(node));
    node->accept(&pass);
    pass.modifier_.modify_ir();
  }
};

namespace irpass {

void eliminate_immutable_local_vars(IRNode *root) {
  TI_AUTO_PROF;
  EliminateImmutableLocalVars::run(root);
}

}  // namespace irpass

}  // namespace taichi::lang
