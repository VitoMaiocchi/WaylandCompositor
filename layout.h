#ifndef VITOWM_LAYOUT_H
#define VITOWM_LAYOUT_H

struct LayoutNode {

    enum NodeType {
        CLIENT,
        HORIZONTAL,
        VERTICAL
    } nodeType;

    struct LayoutNode* next;
    struct LayoutNode* prev;
    struct LayoutNode* root;
    struct LayoutNode* child;
    
};

void layout_init(struct wlr_box output);
void layout_add_client(struct LayoutNode* client);
void layout_remove_client(struct LayoutNode* client);

#endif