#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <assert.h>
#include "layout.h"
#include "wayland.h"

typedef struct LayoutNode Node;

static void layout_arrange(Node* root, struct wlr_box extends) {
    if(!root) return;

    if(root->nodeType == CLIENT) {
        client_set_extends(root, extends);
        return;
    }

    Node* child = root->child;
    if(!child) return;

    int n = 1;
    Node* it_node = child->next;
    //while( (it_node = it_node->next) != child) n++;
    while(it_node != child) {
        n++;
        it_node = it_node->next;
    }

    int first;
    int delta;
    struct wlr_box e;
    it_node = child;

    if(root->nodeType == HORIZONTAL){
        delta = extends.width / n;
        first = extends.width - delta * (n-1);

        e.y = extends.y;
        e.height = extends.height;
        e.x = extends.x;
        e.width = first;

        layout_arrange(it_node, e);
        e.x += first;
        e.width = delta;

        for(int i = 1; i < n; i++) {
            it_node = it_node->next;
            layout_arrange(it_node, e);
            e.x += delta;
        }

    } else if(root->nodeType == VERTICAL) {
        delta = extends.height / n;
        first = extends.height - delta * (n-1);

        e.x = extends.x;
        e.width = extends.width;
        e.y = extends.y;
        e.height = first;

        layout_arrange(it_node, e);
        e.y += first;
        e.height = delta;

        for(int i = 1; i < n; i++) {
            it_node = it_node->next;
            layout_arrange(it_node, e);
            e.y += delta;
        }
    }

}

static void node_add_child(Node* root, Node* child) {
    assert(root);
    assert(child);

    child->root = root;
    if(!root->child) {
        root->child = child;
        child->next = child;
        child->prev = child;
        return;
    }

    Node* c = root->child;
    root->child = child;

    child->next = c;
    child->prev = c->prev;
    c->prev->next = child;
    c->prev = child;
}

/*
removes child node. all parameters must be != 0
@return 0 if child was removed succesfully
*/
static int node_remove_child(Node* root, Node* child) {
    assert(root);
    assert(child);

    if(!root->child) return -1;
    Node* c = root->child;
    Node* it = c;
    if(c == child) {
        if(child->next == child) {
            root->child = NULL;
            return 0;
        }
        root->child = child->next;
        goto remove_child;
    }

    it = c->next;
    while(it != c) {
        if(it == child) goto remove_child;
        it = it->next;
    }
    return -1;

    remove_child:

    c->prev->next = c->next;
    c->next->prev = c->prev;

    return 0;
}

static void node_init(Node* node) {
    node->child = NULL;
    node->next = NULL;
    node->prev = NULL;
    node->root = NULL;
}



static Node root;
static struct wlr_box extends;

void layout_init(struct wlr_box output) {
    root.nodeType = HORIZONTAL;
    node_init(&root);
    extends = output;
}

void layout_add_client(struct LayoutNode* client) {
    client->nodeType = CLIENT;
    node_init(client);
    node_add_child(&root, client);
    layout_arrange(&root, extends);
}

void layout_remove_client(struct LayoutNode* client) {
    node_remove_child(&root, client);
    node_init(client);
    layout_arrange(&root, extends);
}