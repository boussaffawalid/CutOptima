#ifndef ResultH
#define ResultH

#include <memory>
#include <cassert>
#include "types.h"

namespace Denisenko {
namespace Raskroy {

struct OldLayoutResult {
    struct Detail {
        scalar size;
        unsigned num;
        std::list<std::pair<Part *, unsigned> > parts;
    };

	typedef std::vector<Detail> Details;

	unsigned s; // cut goes perpendicular to this axis
	unsigned kratnostj;
	scalar cut;
	Details details;

	OldLayoutResult() : premain(0), precurse(0) {}
	OldLayoutResult(const OldLayoutResult &orig)
		: s(orig.s),
		kratnostj(orig.kratnostj),
		cut(orig.cut),
		details(orig.details),
		premain(orig.premain),
		precurse(orig.precurse)
	{
		orig.premain = 0;
		orig.precurse = 0;
	}

	~OldLayoutResult() {
		delete premain;
		delete precurse;
	}

	void set(int s, unsigned kratnostj, scalar cut, Details &details, OldLayoutResult *premain, OldLayoutResult *precurse);
	OldLayoutResult& operator = (const OldLayoutResult &orig);
	void attachRemain(OldLayoutResult &remain);
	void attachRecurse(OldLayoutResult &recurse);
	OldLayoutResult * watchRemain(void) const {return premain;}
	OldLayoutResult * watchRecurse(void) const {return precurse;}

	void CheckAndCalcStat(scalar cutThickness, const Rect& rect, Stat* outStat) const;

private:
	mutable OldLayoutResult *premain;
	mutable OldLayoutResult *precurse;
};


class Result {
public:
	OldLayoutResult raskroy;
	Parts::iterator sheet;
	Stat Statistics;
	unsigned amount;

	Result(void) : amount(0) {}
};


enum {ALONG_X = 0, ALONG_Y = 1};


// New format of layout results

// Type of the element
enum LayoutElementType {
    ELEM_REMAIN = 0,
    ELEM_CUT = 1,
    ELEM_RECT = 2,  // a layed-out rectangle
    ELEM_SUBLAYOUT = 3,
};


// should be compatible with C
struct LayoutElement
{
    scalar size;  // size of the element along layout axis
    int type;  // rect, remain, cut or sub-layout
    union {
        int rect_index;  // if type = ELEM_RECT this contains index of
                         // the rect from layout_rects
        struct Layout * layout;  // if type == ELEM_SUBLAYOUT this
                                 // is the pointer to sub-layout
    };
    LayoutElement() : type(ELEM_REMAIN), layout(0) {}
};


// Layout result
// should be compatible with C
struct Layout
{
    int along;  // elements are located along: 0 - X, 1 - Y
    size_t num_elements;
    LayoutElement * elements;  // rects, cuts, remains and sub-layouts

    Layout() : num_elements(0), elements(0) {}

    Layout(int along, int num_elements) :
        along(along),
        num_elements(num_elements),
        elements(new LayoutElement[num_elements])
    {
    }

    void clear() {
        for (size_t i = 0; i < num_elements; i++) {
            if (elements[i].type == ELEM_SUBLAYOUT)
                delete elements[i].layout;
        }
        delete [] elements;
        elements = 0;
        num_elements = 0;
    }

    ~Layout() {
        clear();
    }
};


struct LayoutElementBuilder
{
    scalar size;  // size of the element along layout axis
    Rect rect;
    LayoutElementType type;  // rect, remain, cut or sub-layout
    Part * part;
    struct LayoutBuilder * layout;  // if type == ELEM_SUBLAYOUT this
                                 // is the pointer to sub-layout
    LayoutElementBuilder() : type(ELEM_REMAIN), part(nullptr), layout(0) {}

    void _convert(LayoutElement & out) const;
};


struct LayoutBuilder {
    Rect rect;
    int axis;
    std::list<LayoutElementBuilder> elements;
    scalar remain;

    LayoutBuilder() : axis(0) {}

    void simplify();

    void begin_appending();

    void append_sublayout(std::unique_ptr<LayoutBuilder> sublayout, scalar size) {
        assert(sublayout->rect.Size[axis] == size);
        assert(size <= remain);
        if (sublayout->axis == axis) {
            elements.splice(elements.end(), sublayout->elements);
        }
        else {
            LayoutElementBuilder sublayout_el;
            sublayout_el.type = ELEM_SUBLAYOUT;
            sublayout_el.layout = sublayout.release();
            sublayout_el.size = size;
            sublayout_el.rect = rect;
            sublayout_el.rect.Size[axis] = size;
            elements.push_back(sublayout_el);
        }
        remain -= size;
    }

    void append_cut(scalar size) {
        assert(size <= remain);
        LayoutElementBuilder cut_el;
        cut_el.type = ELEM_CUT;
        cut_el.size = size;
        cut_el.rect = rect;
        cut_el.rect.Size[axis] = size;
        elements.push_back(cut_el);
        remain -= size;
    }

    void append_remain(scalar size) {
        assert(size <= remain);
        LayoutElementBuilder remain_el;
        remain_el.type = ELEM_REMAIN;
        remain_el.size = size;
        remain_el.rect = rect;
        remain_el.rect.Size[axis] = size;
        elements.push_back(remain_el);
        remain -= size;
    }

    void append_part(Part * part, scalar size);

    void _free() {
        for (auto i = elements.begin();
             i != elements.end(); i++)
        {
            if (i->type == ELEM_SUBLAYOUT)
                delete i->layout;
        }
    }

    ~LayoutBuilder() {
        _free();
    }

    void to_layout(Layout & out) const {
        out.clear();
        out.along = axis;
        out.num_elements = elements.size();
        out.elements = new LayoutElement[out.num_elements];
        auto i = 0;
        for (auto pel = elements.begin();
             pel != elements.end(); pel++, i++)
        {
            pel->_convert(out.elements[i]);
        }
    }

    void check() const;
};


} // namespace Denisenko
} // namespace Raskroy

#endif // ResultH
