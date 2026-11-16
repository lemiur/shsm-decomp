/* classes_slots.c: who a vtable slot belongs to. classes.json records for each word of a class's table the
 * immediate base whose table has that index (`<- Base`) and the base's own word; that base is only the first step of
 * a chain. This walks the chain: the class that introduces the slot is the topmost one whose table has the index;
 * the class whose function the table holds is the first one, from the class upwards, that defines it (a new slot or
 * an override), so an inherited slot's holder is a base; pure slots are null entries. The name to declare under the
 * project's convention is the introducing slot's `fn_<address>` plain name with the declaring class's mangling:
 * the slot keeps its identity through every override (`fn_8009CB1C__10CActorTestFR7unk_Msg` overrides
 * `fn_8009CB1C__12unk_8001777CFR7unk_Msg`). */
#include "classes.h"

#include <ctype.h>
#include <string.h>

static bool eqz(const char *x, const char *y) { return x && y && strcmp(x, y) == 0; }

const char *classes_slot_legend(void) {
    return "introduced by = the topmost base whose table has this slot; held by = the class whose function the table "
           "holds (a base for an inherited slot); declare = the name under the project's convention: the introducing "
           "slot's fn_<address> plain name with the declaring class's mangling; pure = a null entry";
}

/* "@36@name" -> "name" */
static const char *skip_thunk(const char *s) {
    if (!s || s[0] != '@') return s;
    const char *p = s + 1;
    while (isdigit((unsigned char)*p)) p++;
    return (p > s + 1 && *p == '@') ? p + 1 : s;
}

/* The start of the mangled tail of a member name: the "__" that precedes a class length or a Q-qualification. */
static const char *tail_of(const char *name) {
    if (!name) return NULL;
    for (const char *p = name + 1; *p; p++)
        if (p[0] == '_' && p[1] == '_' && p[-1] != '_' && (isdigit((unsigned char)p[2]) || p[2] == 'Q')) return p;
    return NULL;
}

static const char *plain_of(AvpArena *a, const char *name) {
    const char *t = tail_of(name);
    return t ? avp_cz(a, avp_strn(name, (size_t)(t - name))) : name;
}

static const char *word_target(const ClassesWord *w) { return w->has_thunk && w->thunk_target ? w->thunk_target : w->target; }

const ClassesWord *classes_word_at(const ClassesClass *c, i64 index) {
    if (!c || !c->vtable || index < 0) return NULL;
    const ClassesVtable *vt = c->vtable;
    if ((size_t)index < vt->n_words && vt->words[index].index == index) return &vt->words[index];
    for (size_t i = 0; i < vt->n_words; i++)
        if (vt->words[i].index == index) return &vt->words[i];
    return NULL;
}

bool classes_slot_label(AvpArena *a, const ClassesDb *db, const ClassesClass *c, const ClassesWord *w,
                        ClassesSlotLabel *o) {
    memset(o, 0, sizeof *o);
    if (!w->status || eqz(w->status, "header") || eqz(w->status, "unresolved")) return false;
    o->pure_here = !w->target;
    o->inherited = eqz(w->status, "inherited");
    const ClassesClass *cls = c;
    const ClassesWord *cur = w;
    bool holder_set = false;
    for (int depth = 0; depth < 64; depth++) {
        bool up = (eqz(cur->status, "inherited") || eqz(cur->status, "override")) && cur->base;
        if (!holder_set && !eqz(cur->status, "inherited")) {
            holder_set = true;
            o->holder = cur->target ? cls->name : NULL;
        }
        if (!up) { /* new or null: the slot starts here */
            o->introducer = cls->name;
            o->intro_target = word_target(cur);
            o->pure_intro = !cur->target;
            break;
        }
        const ClassesClass *b = classes_by_name(db, cur->base);
        const ClassesWord *bw = cur->has_base_index ? classes_word_at(b, cur->base_index) : NULL;
        if (!bw || !bw->status || eqz(bw->status, "header") || eqz(bw->status, "unresolved")) {
            /* the base's own table is not in classes.json: the topmost class known */
            o->introducer = cur->base;
            o->intro_target = cur->has_base_target ? cur->base_target : NULL;
            o->pure_intro = cur->has_base_target && !cur->base_target;
            o->topmost_known = true;
            if (!holder_set) o->holder = cur->base;
            break;
        }
        cls = b;
        cur = bw;
    }
    /* A new slot of the primary table that a secondary base's table also holds (as an override, through a thunk) is
     * that base's slot: the base introduces it, this class holds it. */
    if (eqz(w->status, "new") && w->target && c->vtable) {
        const char *mine = skip_thunk(word_target(w));
        for (size_t i = 0; i < c->vtable->n_words; i++) {
            const ClassesWord *x = &c->vtable->words[i];
            if (x == w || !eqz(x->status, "override") || !x->target || !eqz(skip_thunk(word_target(x)), mine)) continue;
            ClassesSlotLabel via;
            if (!classes_slot_label(a, db, c, x, &via) || !via.introducer) continue;
            o->introducer = via.introducer;
            o->intro_target = via.intro_target;
            o->pure_intro = via.pure_intro;
            o->topmost_known = via.topmost_known;
            o->via_offset = x->offset;
            break;
        }
    }
    /* the name to declare */
    const char *own = word_target(w);
    if (!own || o->inherited) return true;
    if (!o->intro_target || (eqz(w->status, "new") && !o->via_offset)) {
        o->declare = skip_thunk(own);
        return true;
    }
    const char *ip = plain_of(a, skip_thunk(o->intro_target));
    const char *op = plain_of(a, skip_thunk(own));
    if (eqz(ip, op) || strncmp(ip, "__", 2) == 0) {
        o->declare = skip_thunk(own);
        return true;
    }
    const char *tail = tail_of(skip_thunk(own));
    o->declare = tail ? avp_sprintf(a, "%s%s", ip, tail) : ip;
    o->mangling_unknown = tail == NULL;
    return true;
}

const char *classes_slot_label_text(AvpArena *a, const ClassesWord *w, const ClassesSlotLabel *l) {
    AvpBuf b;
    avp_buf_init(&b, a);
    avp_buf_printf(&b, "introduced by %s%s%s", l->introducer ? l->introducer : "?", l->pure_intro ? " (pure)" : "",
                   l->topmost_known ? " or above (its table is not in classes.json)" : "");
    if (l->holder) avp_buf_printf(&b, "; held by %s", l->holder);
    else avp_buf_putz(&b, "; null entry in this table");
    const char *shown = skip_thunk(w->target);
    if (l->declare && !eqz(l->declare, shown))
        avp_buf_printf(&b, "; declare %s%s", l->declare, l->mangling_unknown ? " (own mangling unknown)" : "");
    else if (l->inherited) avp_buf_putz(&b, "; inherited unchanged");
    if (l->via_offset) avp_buf_printf(&b, "; also held at %s by the table of a secondary base", l->via_offset);
    return avp_cz(a, avp_buf_str(&b));
}
