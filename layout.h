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

    struct wlr_box extends;
    
};

void layout_init(struct wlr_box output);
void layout_add_client(struct LayoutNode* client);
struct LayoutNode* layout_remove_client(struct LayoutNode* client);
struct LayoutNode* layout_get_client_at_position(const int x, const int y);

#endif