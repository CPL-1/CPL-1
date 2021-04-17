#ifndef __RB_TREE_H_INCLUDED__
#define __RB_TREE_H_INCLUDED__

#include <common/lib/rbtree.h>

struct RedBlackTree_Node {
	bool is_black;
	struct RedBlackTree_Node *parent;
	struct RedBlackTree_Node *desc[2];
	struct RedBlackTree_Node *iter[2];
};

typedef int (*RedBlackTree_Comparator)(struct RedBlackTree_Node *left, struct RedBlackTree_Node *right, void *opaque);
typedef void (*RedBlackTree_CleanupCallback)(struct RedBlackTree_Node *node, void *opaque);
typedef bool (*RedBlackTree_Filter)(struct RedBlackTree_Node *node, void *opaque);

struct RedBlackTree_Tree {
	struct RedBlackTree_Node *root;
	struct RedBlackTree_Node *ends[2];
};

MAYBE_UNUSED static bool RedBlackTree_Insert(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node,
											 RedBlackTree_Comparator comparator, void *ctx);
MAYBE_UNUSED static struct RedBlackTree_Node *RedBlackTree_Query(struct RedBlackTree_Tree *root,
																 struct RedBlackTree_Node *node,
																 RedBlackTree_Comparator comparator, void *ctx,
																 bool require_match);
MAYBE_UNUSED static void RedBlackTree_Remove(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node);
MAYBE_UNUSED static struct RedBlackTree_Node *RedBlackTree_LowerBound(struct RedBlackTree_Tree *root,
																	  RedBlackTree_Filter filter, void *ctx);
MAYBE_UNUSED static struct RedBlackTree_Node *RedBlackTree_UpperBound(struct RedBlackTree_Tree *root,
																	  RedBlackTree_Filter filter, void *ctx);

static struct RedBlackTree_Node *RedBlackTree_GetParent(struct RedBlackTree_Node *node) {
	if (node == NULL) {
		return NULL;
	}
	return node->parent;
}

static bool RedBlackTree_IsBlack(struct RedBlackTree_Node *node) {
	if (node == NULL) {
		return true;
	}
	return node->is_black;
}

static void RedBlackTree_SetIsBlack(struct RedBlackTree_Node *node, bool is_black) {
	if (node != NULL) {
		node->is_black = is_black;
	}
}

static int RedBlackTree_GetDirection(struct RedBlackTree_Node *node) {
	if (node->parent == 0) {
		return 0;
	}
	if (node->parent->desc[0] == node) {
		return 0;
	}
	return 1;
}

static struct RedBlackTree_Node *RedBlackTree_GetSibling(struct RedBlackTree_Node *node) {
	struct RedBlackTree_Node *parent = RedBlackTree_GetParent(node);
	if (parent == NULL) {
		return NULL;
	}
	return parent->desc[1 - RedBlackTree_GetDirection(node)];
}

static void RedBlackTree_Rotate(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node, int direction) {
	struct RedBlackTree_Node *new_top = node->desc[1 - direction];
	struct RedBlackTree_Node *parent = node->parent;
	struct RedBlackTree_Node *middle = new_top->desc[direction];
	int pos = RedBlackTree_GetDirection(node);
	node->desc[1 - direction] = middle;
	if (middle != NULL) {
		middle->parent = node;
	}
	new_top->desc[direction] = node;
	node->parent = new_top;
	if (parent == NULL) {
		root->root = new_top;
		new_top->parent = NULL;
		new_top->is_black = true;
	} else {
		parent->desc[pos] = new_top;
		new_top->parent = parent;
	}
}

static void RedBlackTree_FixInsertion(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node) {
	while (!RedBlackTree_IsBlack(RedBlackTree_GetParent(node))) {
		node->is_black = false;
		struct RedBlackTree_Node *parent = RedBlackTree_GetParent(node);
		struct RedBlackTree_Node *uncle = RedBlackTree_GetSibling(parent);
		struct RedBlackTree_Node *greatparent = RedBlackTree_GetParent(parent);
		int node_pos = RedBlackTree_GetDirection(node);
		if (RedBlackTree_IsBlack(uncle)) {
			if (RedBlackTree_GetDirection(parent) == node_pos) {
				RedBlackTree_Rotate(root, greatparent, 1 - node_pos);
				RedBlackTree_SetIsBlack(parent, true);
			} else {
				RedBlackTree_Rotate(root, parent, 1 - node_pos);
				RedBlackTree_Rotate(root, greatparent, node_pos);
				RedBlackTree_SetIsBlack(node, true);
			}
			RedBlackTree_SetIsBlack(greatparent, false);
			break;
		} else {
			RedBlackTree_SetIsBlack(parent, true);
			RedBlackTree_SetIsBlack(uncle, true);
			RedBlackTree_SetIsBlack(greatparent, false);
			node = greatparent;
		}
	}
	root->root->is_black = true;
}

MAYBE_UNUSED static bool RedBlackTree_Insert(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node,
											 RedBlackTree_Comparator comparator, void *ctx) {
	node->desc[0] = node->desc[1] = node->parent = NULL;
	node->is_black = false;
	if (root->root == NULL) {
		node->is_black = true;
		root->root = node;
		node->parent = NULL;
		root->ends[0] = root->ends[1] = node;
		node->iter[0] = node->iter[1] = NULL;
		return true;
	}
	struct RedBlackTree_Node *prev = NULL;
	struct RedBlackTree_Node *current = root->root;
	int direction = 0;
	while (current != NULL) {
		int cmp_result = comparator(node, current, ctx);
		if (cmp_result == 0) {
			return false;
		}
		direction = (int)(cmp_result == 1);
		prev = current;
		current = current->desc[direction];
	}
	prev->desc[direction] = node;
	node->parent = prev;
	struct RedBlackTree_Node *neighbour = prev->iter[direction];
	if (neighbour != NULL) {
		neighbour->iter[1 - direction] = node;
	} else {
		root->ends[direction] = node;
	}
	prev->iter[direction] = node;
	node->iter[direction] = neighbour;
	node->iter[1 - direction] = prev;
	RedBlackTree_FixInsertion(root, node);
	return true;
}

static INLINE void RedBlackTree_SwapNodes(struct RedBlackTree_Node *node, struct RedBlackTree_Node *replacement,
										  struct RedBlackTree_Tree *root) {
	struct RedBlackTree_Node *node_parent = node->parent;
	int node_pos = RedBlackTree_GetDirection(node);
	struct RedBlackTree_Node *node_child[2];
	node_child[0] = node->desc[0];
	node_child[1] = node->desc[1];
	struct RedBlackTree_Node *replacement_child = replacement->desc[0];
	int replacement_pos = RedBlackTree_GetDirection(replacement);
	bool node_is_black = node->is_black;
	bool replacement_is_black = replacement->is_black;
	node->is_black = replacement_is_black;
	replacement->is_black = node_is_black;
	if (node->desc[0] == replacement) {
		node->desc[0] = replacement_child;
		node->desc[1] = NULL;
		if (replacement_child != NULL) {
			replacement_child->parent = node;
		}
		replacement->desc[0] = node;
		replacement->desc[1] = node_child[1];
		node_child[1]->parent = replacement;
		node->parent = replacement;
		replacement->parent = node_parent;
		if (node_parent == NULL) {
			root->root = replacement;
			replacement->is_black = true;
			replacement->parent = NULL;
		} else {
			node_parent->desc[node_pos] = replacement;
		}
	} else {
		struct RedBlackTree_Node *replacement_parent = replacement->parent;
		replacement->parent = node_parent;
		if (node_parent == NULL) {
			root->root = replacement;
			replacement->is_black = true;
			replacement->parent = NULL;
		} else {
			node_parent->desc[node_pos] = replacement;
		}
		node->parent = replacement_parent;
		replacement_parent->desc[replacement_pos] = node;
		replacement->desc[0] = node_child[0];
		replacement->desc[1] = node_child[1];
		node_child[0]->parent = replacement;
		node_child[1]->parent = replacement;
		node->desc[0] = replacement_child;
		node->desc[1] = NULL;
		if (replacement_child != NULL) {
			replacement_child->parent = node;
		}
	}
	struct RedBlackTree_Node *node_iters[2] = {node->iter[0], node->iter[1]};
	struct RedBlackTree_Node *replacement_iters[2] = {replacement->iter[0], replacement->iter[1]};
	if (node_iters[0] == replacement) {
		node_iters[0] = node;
	}
	if (node_iters[1] == replacement) {
		node_iters[1] = node;
	}
	if (replacement_iters[0] == node) {
		replacement_iters[0] = replacement;
	}
	if (replacement_iters[1] == node) {
		replacement_iters[1] = replacement;
	}
	node->iter[0] = replacement_iters[0];
	node->iter[1] = replacement_iters[1];
	replacement->iter[0] = node_iters[0];
	replacement->iter[1] = node_iters[1];
	if (replacement_iters[0] == NULL) {
		root->ends[0] = node;
	} else {
		replacement_iters[0]->iter[1] = node;
	}
	if (replacement_iters[1] == NULL) {
		root->ends[1] = node;
	} else {
		replacement_iters[1]->iter[0] = node;
	}
	if (node_iters[0] == NULL) {
		root->ends[0] = replacement;
	} else {
		node_iters[0]->iter[1] = replacement;
	}
	if (node_iters[1] == NULL) {
		root->ends[1] = replacement;
	} else {
		node_iters[1]->iter[0] = replacement;
	}
}

MAYBE_UNUSED static struct RedBlackTree_Node *RedBlackTree_Query(struct RedBlackTree_Tree *root,
																 struct RedBlackTree_Node *node,
																 RedBlackTree_Comparator comparator, void *ctx,
																 bool require_match) {
	if (root->root == NULL) {
		return NULL;
	}
	struct RedBlackTree_Node *prev = NULL;
	struct RedBlackTree_Node *current = root->root;
	while (current != NULL) {
		int cmp_result = comparator(node, current, ctx);
		if (cmp_result == 0) {
			return current;
		}
		int direction = (int)(cmp_result == 1);
		prev = current;
		current = current->desc[direction];
	}
	return require_match ? NULL : prev;
}

static struct RedBlackTree_Node *RedBlackTree_FindReplacement(struct RedBlackTree_Node *node) {
	struct RedBlackTree_Node *current = node->desc[0];
	while (current->desc[1] != NULL) {
		current = current->desc[1];
	}
	return current;
}

static void RedBlackTree_RemoveInternalNodes(struct RedBlackTree_Node *node, struct RedBlackTree_Tree *root) {
	if (node->desc[0] != NULL && node->desc[1] != NULL) {
		struct RedBlackTree_Node *prev = RedBlackTree_FindReplacement(node);
		RedBlackTree_SwapNodes(node, prev, root);
	}
}

static void RedBlackTree_FixDoubleBlack(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node) {
	while (true) {
		if (node->parent == NULL) {
			return;
		}
		int node_pos = RedBlackTree_GetDirection(node);
		struct RedBlackTree_Node *node_parent = node->parent;
		struct RedBlackTree_Node *node_sibling = RedBlackTree_GetSibling(node);
		if (!RedBlackTree_IsBlack(node_sibling)) {
			RedBlackTree_Rotate(root, node_parent, node_pos);
			RedBlackTree_SetIsBlack(node_sibling, true);
			RedBlackTree_SetIsBlack(node_parent, false);
			node_sibling = RedBlackTree_GetSibling(node);
		}
		if (node_sibling != NULL) {
			if (RedBlackTree_IsBlack(node_sibling->desc[0]) && RedBlackTree_IsBlack(node_sibling->desc[1])) {
				if (RedBlackTree_IsBlack(node_parent)) {
					RedBlackTree_SetIsBlack(node_sibling, false);
					node = node_parent;
					continue;
				} else {
					RedBlackTree_SetIsBlack(node_parent, true);
					RedBlackTree_SetIsBlack(node_sibling, false);
					return;
				}
			}
		}
		bool parent_is_black = node_parent->is_black;
		if (RedBlackTree_IsBlack(node_sibling->desc[1 - node_pos])) {
			RedBlackTree_Rotate(root, node_sibling, 1 - node_pos);
			node_sibling->is_black = false;
			node_sibling = RedBlackTree_GetSibling(node);
			node_sibling->is_black = true;
		}
		RedBlackTree_Rotate(root, node_parent, node_pos);
		node_parent->is_black = true;
		node_sibling->is_black = parent_is_black;
		if (node_sibling->desc[1 - node_pos] != NULL) {
			node_sibling->desc[1 - node_pos]->is_black = true;
		}
		return;
	}
}

static void RedBlackTree_CutFromIterList(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node) {
	if (node->iter[0] != NULL) {
		node->iter[0]->iter[1] = node->iter[1];
	} else {
		root->ends[0] = node->iter[1];
		if (node->iter[1] != NULL) {
			root->ends[0]->iter[0] = NULL;
		}
	}
	if (node->iter[1] != NULL) {
		node->iter[1]->iter[0] = node->iter[0];
	} else {
		root->ends[1] = node->iter[0];
		if (node->iter[0] != NULL) {
			root->ends[1]->iter[1] = NULL;
		}
	}
}

MAYBE_UNUSED static void RedBlackTree_Remove(struct RedBlackTree_Tree *root, struct RedBlackTree_Node *node) {
	RedBlackTree_RemoveInternalNodes(node, root);
	struct RedBlackTree_Node *node_child = node->desc[0];
	struct RedBlackTree_Node *node_parent = node->parent;
	int node_pos = RedBlackTree_GetDirection(node);
	if (node_child == NULL) {
		node_child = node->desc[1];
	}
	if (node_child != NULL) {
		if (node_parent == NULL) {
			root->root = node_child;
			node_child->is_black = true;
			node_child->parent = NULL;
		} else {
			node_parent->desc[node_pos] = node_child;
			node_child->parent = node_parent;
		}
		RedBlackTree_SetIsBlack(node_child, true);
		RedBlackTree_CutFromIterList(root, node);
		return;
	}
	if (!RedBlackTree_IsBlack(node)) {
		node_parent->desc[node_pos] = NULL;
		RedBlackTree_CutFromIterList(root, node);
		return;
	}
	RedBlackTree_FixDoubleBlack(root, node);
	if (node_parent == NULL) {
		root->root = NULL;
		RedBlackTree_CutFromIterList(root, node);
	} else {
		node_parent->desc[node_pos] = NULL;
		RedBlackTree_CutFromIterList(root, node);
	}
}

MAYBE_UNUSED static void RedBlackTree_Clear(struct RedBlackTree_Tree *root, RedBlackTree_CleanupCallback callback,
											void *callbackOpaque) {
	struct RedBlackTree_Node *current = root->ends[0];
	while (current != NULL) {
		struct RedBlackTree_Node *next = current->iter[1];
		callback(current, callbackOpaque);
		current = next;
	}
	root->root = root->ends[0] = root->ends[1] = NULL;
}

static int RedBlackTree_CheckBlackCountsProperty(struct RedBlackTree_Node *node) {
	if (node == NULL) {
		return 0;
	}
	int left_height = RedBlackTree_CheckBlackCountsProperty(node->desc[0]);
	int right_height = RedBlackTree_CheckBlackCountsProperty(node->desc[1]);
	if (left_height == -1 || right_height == -1) {
		return -1;
	}
	if (right_height != left_height) {
		return -1;
	}
	return left_height + (node->is_black ? 1 : 0);
}

static bool RedBlackTree_CheckParents(struct RedBlackTree_Node *node) {
	if (node == NULL) {
		return true;
	}
	bool left_ok = RedBlackTree_CheckParents(node->desc[0]);
	bool right_ok = RedBlackTree_CheckParents(node->desc[1]);
	if (!(left_ok && right_ok)) {
		return false;
	}
	if (node->desc[0] != NULL && node->desc[0]->parent != node) {
		return false;
	}
	if (node->desc[1] != NULL && node->desc[1]->parent != node) {
		return false;
	}
	return true;
}

static bool RedBlackTree_CheckDoubleRedAbsence(struct RedBlackTree_Node *node) {
	if (RedBlackTree_IsBlack(node)) {
		return true;
	}
	bool left_ok = RedBlackTree_CheckParents(node->desc[0]);
	bool right_ok = RedBlackTree_CheckParents(node->desc[1]);
	if (!(left_ok && right_ok)) {
		return false;
	}
	if (!RedBlackTree_IsBlack(node->desc[0]) || !RedBlackTree_IsBlack(node->desc[1]) ||
		!RedBlackTree_IsBlack(node->parent)) {
		return false;
	}
	return true;
}

static bool RedBlackTree_VerifyIterators(struct RedBlackTree_Tree *root, RedBlackTree_Comparator cmp, void *opaque,
										 bool require_neg_one) {
	struct RedBlackTree_Node *current = root->ends[0];
	while (current != NULL) {
		struct RedBlackTree_Node *next = current->iter[1];
		if (next != NULL) {
			if (next->iter[0] != current) {
				return false;
			}
		} else {
			current = next;
			continue;
		}
		if (cmp == NULL) {
			continue;
		}
		int cmp_result = cmp(current, next, opaque);
		if (require_neg_one) {
			if (cmp_result != -1) {
				return false;
			}
		} else {
			if (cmp_result == 1) {
				return false;
			}
		}
		current = next;
	}
	return true;
}

static bool RedBlackTree_VerifyBSTProperty(struct RedBlackTree_Node *node, RedBlackTree_Comparator cmp, void *opaque,
										   bool require_neg_one) {
	if (node == NULL) {
		return true;
	}
	bool left_ok = RedBlackTree_VerifyBSTProperty(node->desc[0], cmp, opaque, require_neg_one);
	bool right_ok = RedBlackTree_VerifyBSTProperty(node->desc[1], cmp, opaque, require_neg_one);
	if (!(left_ok && right_ok)) {
		return false;
	}
	if (node->desc[0] != NULL) {
		int cmp_result = cmp(node->desc[0], node, opaque);
		if (require_neg_one) {
			if (cmp_result != -1) {
				return false;
			}
		} else {
			if (cmp_result == 1) {
				return false;
			}
		}
	}
	if (node->desc[1] != NULL) {
		int cmp_result = cmp(node, node->desc[1], opaque);
		if (require_neg_one) {
			if (cmp_result != -1) {
				return false;
			}
		} else {
			if (cmp_result == 1) {
				return false;
			}
		}
	}
	return true;
}

MAYBE_UNUSED static bool RedBlackTree_VerifyInvariants(struct RedBlackTree_Tree *root, RedBlackTree_Comparator cmp,
													   void *opaque, bool require_neg_one) {
	if (RedBlackTree_CheckBlackCountsProperty(root->root) == -1) {
		return false;
	}
	if (!RedBlackTree_CheckParents(root->root)) {
		return false;
	}
	if (root->root != NULL) {
		if (root->root->parent != NULL) {
			return false;
		}
		if (!(root->root->is_black)) {
			return false;
		}
	}
	if ((root->ends[0] != NULL && root->ends[0]->iter[0] != NULL) ||
		(root->ends[1] != NULL && root->ends[1]->iter[1] != NULL)) {
		return false;
	}
	if (!RedBlackTree_CheckDoubleRedAbsence(root->root)) {
		return false;
	}
	if (!RedBlackTree_VerifyIterators(root, cmp, opaque, require_neg_one)) {
		return false;
	}
	if (cmp != NULL) {
		if (!RedBlackTree_VerifyBSTProperty(root->root, cmp, opaque, require_neg_one)) {
			return false;
		}
	}
	return true;
}

static struct RedBlackTree_Node *RedBlackTree_Bound(struct RedBlackTree_Tree *root, RedBlackTree_Filter filter,
													void *opaque, int dir) {
	struct RedBlackTree_Node *candidate = NULL;
	struct RedBlackTree_Node *current = root->root;
	while (current != NULL) {
		if (filter(current, opaque)) {
			candidate = current;
			current = current->desc[dir];
		} else {
			current = current->desc[1 - dir];
		}
	}
	return candidate;
}

static struct RedBlackTree_Node *RedBlackTree_UpperBound(struct RedBlackTree_Tree *root, RedBlackTree_Filter filter,
														 void *opaque) {
	return RedBlackTree_Bound(root, filter, opaque, 1);
}

static struct RedBlackTree_Node *RedBlackTree_LowerBound(struct RedBlackTree_Tree *root, RedBlackTree_Filter filter,
														 void *opaque) {
	return RedBlackTree_Bound(root, filter, opaque, 0);
}

MAYBE_UNUSED static void RedBlackTree_Initialize(struct RedBlackTree_Tree *tree) {
	tree->ends[0] = tree->ends[1] = tree->root = NULL;
}

#endif
