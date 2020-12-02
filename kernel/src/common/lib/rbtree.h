#ifndef __RB_TREE_H_INCLUDED__
#define __RB_TREE_H_INCLUDED__

#include <common/misc/utils.h>

struct rb_node {
	bool is_black;
	struct rb_node *parent;
	struct rb_node *desc[2];
	struct rb_node *iter[2];
};

typedef int (*rb_cmp_t)(struct rb_node *left, struct rb_node *right,
						const void *opaque);
typedef void (*rb_augment_callback_t)(struct rb_node *parent);
typedef void (*rb_cleanup_callback_t)(struct rb_node *node);

struct rb_root {
	struct rb_node *root;
	struct rb_node *ends[2];
	rb_augment_callback_t augment_callback;
};

static bool rb_insert(struct rb_root *root, struct rb_node *node,
					  rb_cmp_t comparator, void *ctx);
static struct rb_node *rb_query(struct rb_root *root, struct rb_node *node,
								rb_cmp_t comparator, void *ctx,
								bool require_match);
static void rb_delete(struct rb_root *root, struct rb_node *node);

static struct rb_node *rb_get_parent(struct rb_node *node) {
	if (node == NULL) {
		return NULL;
	}
	return node->parent;
}

static bool rb_is_black(struct rb_node *node) {
	if (node == NULL) {
		return true;
	}
	return node->is_black;
}

static void rb_set_is_black(struct rb_node *node, bool is_black) {
	if (node != NULL) {
		node->is_black = is_black;
	}
}

int rb_get_direction(struct rb_node *node) {
	if (node->parent == 0) {
		return 0;
	}
	if (node->parent->desc[0] == node) {
		return 0;
	}
	return 1;
}

static struct rb_node *rb_get_sibling(struct rb_node *node) {
	struct rb_node *parent = rb_get_parent(node);
	if (parent == NULL) {
		return NULL;
	}
	return parent->desc[1 - rb_get_direction(node)];
}

static void rb_rotate(struct rb_root *root, struct rb_node *node,
					  int direction) {
	struct rb_node *new_top = node->desc[1 - direction];
	struct rb_node *parent = node->parent;
	struct rb_node *middle = new_top->desc[direction];
	int pos = rb_get_direction(node);
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

static void rb_fix_insertion(struct rb_root *root, struct rb_node *node) {
	while (!rb_is_black(rb_get_parent(node))) {
		struct rb_node *parent = rb_get_parent(node);
		struct rb_node *uncle = rb_get_sibling(parent);
		struct rb_node *greatparent = rb_get_parent(parent);
		int node_pos = rb_get_direction(node);
		if (rb_is_black(uncle)) {
			if (rb_get_direction(parent) == node_pos) {
				rb_rotate(root, greatparent, 1 - node_pos);
				rb_set_is_black(parent, true);
			} else {
				rb_rotate(root, parent, node_pos);
				rb_rotate(root, greatparent, 1 - node_pos);
				rb_set_is_black(node, true);
			}
			rb_set_is_black(greatparent, false);
			break;
		} else {
			rb_set_is_black(parent, true);
			rb_set_is_black(uncle, true);
			rb_set_is_black(greatparent, false);
			node = greatparent;
		}
	}
	root->root->is_black = true;
}

static bool rb_insert(struct rb_root *root, struct rb_node *node,
					  rb_cmp_t comparator, void *ctx) {
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
	struct rb_node *prev = NULL;
	struct rb_node *current = root->root;
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
	struct rb_node *neighbour = prev->iter[direction];
	if (neighbour != NULL) {
		neighbour->iter[1 - direction] = node;
	} else {
		root->ends[direction] = node;
	}
	prev->iter[direction] = node;
	node->iter[direction] = neighbour;
	node->iter[1 - direction] = prev;
	rb_fix_insertion(root, node);
	if (root->augment_callback != NULL) {
		root->augment_callback(prev);
	}
	return true;
}

static inline void rb_swap(struct rb_node *node, struct rb_node *replacement,
						   struct rb_root *root) {
	struct rb_node *node_parent = node->parent;
	int node_pos = rb_get_direction(node);
	struct rb_node *node_child[2];
	node_child[0] = node->desc[0];
	node_child[1] = node->desc[1];
	struct rb_node *replacement_child = replacement->desc[0];
	int replacement_pos = rb_get_direction(replacement);
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
		struct rb_node *replacement_parent = replacement->parent;
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
	struct rb_node *node_iters[2] = {node->iter[0], node->iter[1]};
	struct rb_node *replacement_iters[2] = {replacement->iter[0],
											replacement->iter[1]};
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

static struct rb_node *rb_query(struct rb_root *root, struct rb_node *node,
								rb_cmp_t comparator, void *ctx,
								bool require_match) {
	if (root->root == NULL) {
		return NULL;
	}
	struct rb_node *prev = NULL;
	struct rb_node *current = root->root;
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

static struct rb_node *rb_find_prev_node(struct rb_node *node) {
	struct rb_node *current = node->desc[0];
	while (current->desc[1] != NULL) {
		current = current->desc[1];
	}
	return current;
}

static void rb_remove_internal_nodes(struct rb_node *node,
									 struct rb_root *root) {
	if (node->desc[0] != NULL && node->desc[1] != NULL) {
		struct rb_node *prev = rb_find_prev_node(node);
		rb_swap(node, prev, root);
	}
}

static void rb_fix_double_black(struct rb_root *root, struct rb_node *node) {
	while (true) {
		if (node->parent == NULL) {
			return;
		}
		int node_pos = rb_get_direction(node);
		struct rb_node *node_parent = node->parent;
		struct rb_node *node_sibling = rb_get_sibling(node);
		if (!rb_is_black(node_sibling)) {
			rb_rotate(root, node_parent, node_pos);
			rb_set_is_black(node_sibling, true);
			rb_set_is_black(node_parent, false);
			node_sibling = rb_get_sibling(node);
		}
		if (node_sibling != NULL) {
			if (rb_is_black(node_sibling->desc[0]) &&
				rb_is_black(node_sibling->desc[1])) {
				if (rb_is_black(node_parent)) {
					rb_set_is_black(node_sibling, false);
					node = node_parent;
					continue;
				} else {
					rb_set_is_black(node_parent, true);
					rb_set_is_black(node_sibling, false);
					return;
				}
			}
		}
		bool parent_is_black = node_parent->is_black;
		if (rb_is_black(node_sibling->desc[1 - node_pos])) {
			rb_rotate(root, node_sibling, 1 - node_pos);
			node_sibling->is_black = false;
			node_sibling = rb_get_sibling(node);
			node_sibling->is_black = true;
		}
		rb_rotate(root, node_parent, node_pos);
		node_parent->is_black = true;
		node_sibling->is_black = parent_is_black;
		if (node_sibling->desc[1 - node_pos] != NULL) {
			node_sibling->desc[1 - node_pos]->is_black = true;
		}
		return;
	}
}

static void rb_cut_from_iter_list(struct rb_root *root, struct rb_node *node) {
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

static void rb_delete(struct rb_root *root, struct rb_node *node) {
	rb_remove_internal_nodes(node, root);
	struct rb_node *node_child = node->desc[0];
	struct rb_node *node_parent = node->parent;
	int node_pos = rb_get_direction(node);
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
			if (root->augment_callback != NULL) {
				root->augment_callback(node_parent);
			}
		}
		rb_set_is_black(node_child, true);
		rb_cut_from_iter_list(root, node);
		return;
	}
	if (!rb_is_black(node)) {
		node_parent->desc[node_pos] = NULL;
		rb_cut_from_iter_list(root, node);
		if (root->augment_callback != NULL) {
			root->augment_callback(node_parent);
		}
		return;
	}
	rb_fix_double_black(root, node);
	if (node_parent == NULL) {
		root->root = NULL;
		rb_cut_from_iter_list(root, node);
	} else {
		node_parent->desc[node_pos] = NULL;
		rb_cut_from_iter_list(root, node);
		if (root->augment_callback != NULL) {
			root->augment_callback(node_parent);
		}
	}
}

static void rb_clear(struct rb_root *root, rb_cleanup_callback_t callback) {
	struct rb_node *current = root->ends[0];
	while (current != NULL) {
		struct rb_node *next = current->iter[1];
		callback(current);
		current = next;
	}
	root->root = root->ends[0] = root->ends[1] = NULL;
}

#endif