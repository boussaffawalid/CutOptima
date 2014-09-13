#include "stdafx.h"
#include <algorithm>
#include "layout2d.h"

namespace Denisenko {
namespace Raskroy {

// Cut sheet by lenght and by width, returns best result
// Parameters:
//		[i] rect - sheet size
//		[o] stat - statistics
//		[o] raskroy - sheet layout
//		[o] rashod - parts consumption
//
inline bool Layout2d::Optimize(const Rect &rect, Stat &stat, int s, OldLayoutResult &raskroy, Amounts &rashod)
{
	// Try to layout using s sizes
	Stat stat1;
	if (Recursion(m_sizes[s].begin(), rect, stat1, s, raskroy, rashod))
	{
		// If success then try to layout using !s sizes
#ifdef _DEBUG
		Stat checkStat;
		raskroy.CheckAndCalcStat(m_layout1d.get_SawThickness(), rect, &checkStat);
		if(!checkStat.IsEqual(stat1))
		{
			raskroy.CheckAndCalcStat(m_layout1d.get_SawThickness(), rect, &checkStat);
			assert(checkStat.IsEqual(stat1));
		}
#endif
		Amounts rashod2(rashod.size());
		OldLayoutResult raskroy2;
		Stat stat2;
		if (Recursion(m_sizes[!s].begin(), rect, stat2, !s, raskroy2, rashod2)
			&& /*pcriteria->quality(*/stat1/*)*/ < /*pcriteria->quality(*/stat2/*)*/)
		{
			// If success and is better than s result than return it
#ifdef _DEBUG
			Stat checkStat;
			raskroy2.CheckAndCalcStat(m_layout1d.get_SawThickness(), rect, &checkStat);
			if(!checkStat.IsEqual(stat2))
				assert(checkStat.IsEqual(stat2));
#endif
			stat = stat2;
			raskroy = raskroy2;
			rashod = rashod2;
		}
		else
		{
			stat = stat1;
		}
		// Otherwise use s result
		return true;
	}
	// Otherwise there is no layout
	return false;
}


bool Layout2d::new_optimize(const Rect &rect, LayoutBuilder &layout)
{
    // choose best (biggest) size to start with
    const Size * best_by[2] = {0};
    for (int i = 0; i <= 1; i++) {
        // get biggest size that fits by i axis
        for (auto sizei = m_sizes[i].begin();
             sizei != m_sizes[i].end(); sizei++)
        {
            if (sizei->Value <= rect.Size[i]) {
                // make shure that there is a rectangle
                // that fits along other axis
                auto fits = false;
                for (auto osi = sizei->other_sizes.begin();
                     osi != sizei->other_sizes.end(); osi++)
                {
                    if (osi->Value <= rect.Size[!i] && 
                        std::any_of(osi->parts.begin(), osi->parts.end(), [this](Part * part){ return (*m_remains)[part->AmountOffset] > 0; })
                        )
                    {
                        fits = true;
                        break;
                    }
                }
                if (fits) {
                    best_by[i] = &*sizei;
                    break;
                }
            }
        }
    }
    int best_parts_axis;
    const Size * best_size;
    if (!best_by[0] || !best_by[1]) {
        return false;
    } else if (best_by[0] && best_by[1]) {
        // if both axises match, choose best of them
        // best is the one with biggest usage ratio to sheet
        if (double(best_by[0]->Value) / double(rect.Size[0]) >= double(best_by[1]->Value) / double(rect.Size[1])) {
            best_parts_axis = 1;
            best_size = best_by[0];
        } else {
            best_parts_axis = 0;
            best_size = best_by[1];
        }
    } else if (best_by[0]) {
        best_parts_axis = 1;
        best_size = best_by[0];
    } else {
        best_parts_axis = 0;
        best_size = best_by[1];
    }

    // do 1D bin packing optimization
    double cut;
    Amounts consumption(m_remains->size());
    scalar remain;
    OldLayoutResult::Details details;
    if (!m_layout1d.Make(*best_size, rect.Size[best_parts_axis], details, consumption, remain, cut))
        return false;

    *m_remains -= consumption;

    scalar saw_size = m_layout1d.get_SawThickness();
    Rect parts_block;
    parts_block.Size[!best_parts_axis] = best_size->Value;
    parts_block.Size[best_parts_axis] = rect.Size[best_parts_axis] - remain - std::min(saw_size, remain);
    scalar remain_bottom_height = rect.Size[1] - parts_block.Size[1] - saw_size;
    scalar remain_right_width = rect.Size[0] - parts_block.Size[0] - saw_size;

    // choose "best" main cut direction, along 0 or 1 axis
    // best is the one that produce remaining rect with biggest square
    // consider cut along x (0) axis
    double remain_x_bottom = double(rect.Size[0]) * double(remain_bottom_height);
    double remain_x_right =  double(remain_right_width) * double(parts_block.Size[1]);
    double max_remain_x = std::max(remain_x_bottom, remain_x_right);
    // consider cut along y (1) axis
    double remain_y_bottom = double(parts_block.Size[0]) * double(remain_bottom_height);
    double remain_y_right = double(remain_right_width) * double(rect.Size[1]);
    double max_remain_y = std::max(remain_y_bottom, remain_y_right);
    int x_axis;
    if (max_remain_x >= max_remain_y) {
        // do layout along Y axis, main cut along X axis
        x_axis = 0;
    } else {
        // do layout along X axis, main cut along Y axis
        // transpose coordinates
        x_axis = 1;
    }

    // comments below assumes axis = 1, if axis = 0 comments are true
    // if you transpose coordinates

    // best main cut is along x axis
    int y_axis = !x_axis;
    layout.axis = y_axis;
    layout.rect = rect;
    layout.begin_appending();
    scalar remain_x = rect.Size[x_axis];
    scalar remain_y = rect.Size[y_axis];

    // horizontal sub-layout for top part containing details
    std::unique_ptr<LayoutBuilder> top_layout(new LayoutBuilder);
    top_layout->axis = x_axis;
    top_layout->rect = rect;
    top_layout->begin_appending();
    top_layout->rect.Size[y_axis] = parts_block.Size[y_axis];

    // parts sub-sub-layout
    std::unique_ptr<LayoutBuilder> pparts_layout(new LayoutBuilder);
    pparts_layout->axis = best_parts_axis;
    pparts_layout->rect = parts_block;
    pparts_layout->begin_appending();
    for (auto deti = details.begin();
            deti != details.end(); deti++)
    {
        for (auto parti = deti->parts.begin();
             parti != deti->parts.end(); parti++)
        {
            auto ppart = parti->first;
            auto amount = parti->second;
            for (; amount > 0; amount--) {
                pparts_layout->append_part(ppart, deti->size);

                // adding cut element
                if (pparts_layout->remain > 0) {
                    auto cut_size = std::min(saw_size, pparts_layout->remain);
                    pparts_layout->append_cut(cut_size);
                }
            }
        }
    }
    top_layout->append_sublayout(std::move(pparts_layout), parts_block.Size[x_axis]);
    assert(parts_block.Size[x_axis] <= remain_x);
    remain_x -= parts_block.Size[x_axis];

    if (remain_x > 0) {
        // vertical cut separating parts block and right remain
        auto cut_size = std::min(saw_size, remain_x);
        top_layout->append_cut(cut_size);
        remain_x -= cut_size;

        if (remain_x > 0) {
            // sublayout for right remain
            Rect remain_right;
            remain_right.Size[x_axis] = remain_x;
            remain_right.Size[y_axis] = parts_block.Size[y_axis];
            std::unique_ptr<LayoutBuilder> pright_layout(new LayoutBuilder);
            if (new_optimize(remain_right, *pright_layout)) {
                top_layout->append_sublayout(std::move(pright_layout), remain_x);
            } else {
                top_layout->append_remain(remain_x);
            }
        }
    }

    // adding top sub-layout to resulting layout
    layout.append_sublayout(std::move(top_layout), parts_block.Size[y_axis]);
    remain_y -= parts_block.Size[y_axis];
    assert(remain_y >= 0);

    if (remain_y > 0) {
        // horizontal cut separating top and bottom remain
        auto cut_size = std::min(saw_size, remain_y);
        layout.append_cut(cut_size);
        remain_y -= cut_size;

        if (remain_y > 0) {
            // create layout for bottom part
            std::unique_ptr<LayoutBuilder> pbottom_layout(new LayoutBuilder);
            Rect remain_bottom;
            remain_bottom.Size[x_axis] = rect.Size[x_axis];
            remain_bottom.Size[y_axis] = remain_y;
            if (new_optimize(remain_bottom, *pbottom_layout)) {
                layout.append_sublayout(std::move(pbottom_layout), remain_y);
            } else {
                layout.append_remain(remain_y);
            }
        }
    }
    return true;
}


class NestingCounterGuard
{
public:
	NestingCounterGuard(int* counterPtr) : m_counterPtr(counterPtr) { (*m_counterPtr)++; }
	~NestingCounterGuard() { (*m_counterPtr)--; }

private:
	int* m_counterPtr;
};

class CompletedCounterGuard
{
public:
	CompletedCounterGuard(int* nestingPtr, int* counterPtr)
		: m_counterPtr(counterPtr), m_nestingPtr(nestingPtr)
	{
	}

	~CompletedCounterGuard()
	{
		if((*m_nestingPtr) == 1)
		{
			(*m_counterPtr)++;
		}
	}
private:
	int* m_counterPtr;
	int* m_nestingPtr;
};

// Recursively try all layouts using length/width (s=0/1)
// Parameters:
//		[i] list - sheet size
//		[o] stat - statistics
//		[i] s - direction
//		[o] raskroy - layout result
//		[o] rashod - details consumption
//
bool Layout2d::Recursion(Sizes::iterator begin, const Rect &rect, Stat &stat, int s, OldLayoutResult &raskroy, Amounts &rashod)
{
	NestingCounterGuard nestingCounterGuard(&m_nesting);

	if (begin == m_sizes[s].end())
	{
		// it is possible to have infinite loop here
		return Recursion(m_sizes[!s].begin(), rect, stat, !s, raskroy, rashod);
	}

	bool first = true;
	Stat bestStat;	// best stat inside loop, will be combined with resulting stat on exit

    // variables are here to save on initialization time
	Amounts rashodPerebor(rashod.size());
	Amounts vrashod(rashod.size());
	Amounts rashod1(rashod.size());
	OldLayoutResult remainRaskroy;	
	OldLayoutResult recurseRaskroy;
	OldLayoutResult::Details details;
	for (Sizes::iterator i = begin; i != m_sizes[s].end(); i++)
	{
		CompletedCounterGuard completedCounterGuard(&m_nesting, &m_completedCounter);

        // if size is too big then terminate loop, other sizes will be
        // bigger
		if (i->Value > rect.Size[s])
			break;

		double opilki;
		scalar remain;
		details.clear();
		if (!m_layout1d.Make(*i, rect.Size[!s], details, rashodPerebor, remain, opilki))
			continue;

		//stat1.sum_cut_length += rect.size[!s];
		// Add sawdust
		double opilki1 = opilki + (double)(rect.Size[!s] - remain) * (double)m_layout1d.get_SawThickness();
		double opilki2 = (double)remain * (double)m_layout1d.get_SawThickness();
		// Calculating remaining rectangle
		Rect remainRect;
		remainRect.Size[s] = i->Value;
		remainRect.Size[!s] = remain;
		// Calculating recursion rectangle
		Rect recurseRect(rect);
		// Recursion rectanble will be reduced by this value
		scalar reduce = i->Value + m_layout1d.get_SawThickness();

		// Calculating multiplicity
		int maxKratnostj = int((rect.Size[s] + m_layout1d.get_SawThickness()) / (i->Value + m_layout1d.get_SawThickness()));
		if (maxKratnostj > 1)
		{
			int kolKrat = *m_remains / rashodPerebor;
			if (maxKratnostj > kolKrat)
				maxKratnostj = kolKrat;
		}

		Stat stat1;
		/*stat1.useful_remain = 0;
		stat1.unuseful_remain = 0;
		stat1.useful_num = 0;*/
		for (int kratnostj = 1; kratnostj <= maxKratnostj; kratnostj++)
		{
			stat1.MakeZero();
            // Modify consumption according to multiplicity
			if (kratnostj > 1)
			{
				rashod1 = rashodPerebor * kratnostj;
				remainRect.Size[s] += m_layout1d.get_SawThickness() + i->Value;
			}
			else if(kratnostj == 1)
			{
				rashod1 = rashodPerebor;
			}
			recurseRect.Size[s] -= reduce;
			stat1.Opilki = opilki1 * (double)kratnostj + opilki2;
			if (recurseRect.Size[s] < 0)
			{
				stat1.Opilki += (double)rect.Size[!s] * (double)recurseRect.Size[s];
				recurseRect.Size[s] = 0;
			}

            // Doing layout for remain rectangle
            // Modifying remains according to layout
			*m_remains -= rashod1;	// should be restored for the next iteration
			Stat remainStat;
			bool haveRemain = Optimize(remainRect, remainStat, !s, remainRaskroy, vrashod);
			if (haveRemain)
			//if (Recursion(sizes[!s].begin(), rect1, stat1, !s, remain_raskroy, rashod))
			{
				// If have layout then need to correct remains and consumption
				stat1 += remainStat;
				rashod1 += vrashod;
				*m_remains -= vrashod;
			}
			else
			{
				stat1.AddScrap(remainRect);
			}

			//if (!first && pcriteria->compare(&best_stat, &stat1))	// already bad
			//	continue;

			// Doing recursion part
			Stat recurseStat;
			bool haveRecurse = Recursion(i + 1, recurseRect, recurseStat, s, recurseRaskroy, vrashod);
			if(haveRecurse)
			{
#ifdef _DEBUG
				Stat checkStat;
				recurseRaskroy.CheckAndCalcStat(m_layout1d.get_SawThickness(), recurseRect, &checkStat);
				if(!checkStat.IsEqual(recurseStat))
				{
					//recurseRaskroy.CheckAndCalcStat(m_layout1d.get_SawThickness(), recurseRect, &checkStat);
					assert(checkStat.IsEqual(recurseStat));
				}
#endif
				stat1 += recurseStat;
			}
			else
			{
				stat1.AddScrap(recurseRect);
			}
			*m_remains += rashod1;	// restoring remains

			// if result is better than current best or is first result then...
			if (bestStat < stat1 || first)
			{
				bestStat = stat1;
				raskroy.set(s,
					kratnostj,
					i->Value, details,
					haveRemain ? &remainRaskroy : 0,
					haveRecurse ? &recurseRaskroy : 0);

				// rashod1 - consumption in remain rectangle
				// vrashod - cunsumption in recursion rectangle
				rashod = rashod1;
				if (haveRecurse)
					rashod += vrashod;
				first = false;
			}
		}
		if (!first)
			break;
	}
	// If had result
	if (!first)
	{
		// Add best stat to resulting stat
		stat = bestStat;
		// Layout and consumption output parameters are already set
		return true;
	}
	return false;
}

} // namespace Denisenko
} // namespace Raskroy
