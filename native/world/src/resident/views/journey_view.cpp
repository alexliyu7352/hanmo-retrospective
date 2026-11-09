// 有限页工厂只检查显示边界，不复制领域validator或重新audit世界。
#include "journey_view.hpp"

namespace hanmo::resident {
Outcome<std::shared_ptr<const JourneyViewPage>> make_journey_page(const WorldIdentity& identity,const StableId& actor,Integer cursor,Integer total,const JourneyRowReader& read) {
    const auto fail=[](const std::string& code)->Outcome<std::shared_ptr<const JourneyViewPage>> {return {false,{},{{code,"journey_view"},ErrorKind::rule,{},{},{},{}}};};
    if(cursor<0||total<0||cursor>total) return fail("InvalidJourneyCursor");
    if(actor.size()>4096) return fail("JourneyViewTextLimit");
    auto page=std::make_shared<JourneyViewPage>(); page->identity=identity; page->actor_id=actor; page->cursor=cursor; page->total_rows=total;
    const Integer count=std::min<Integer>(total-cursor,static_cast<Integer>(journey_page_limit));
    page->rows.reserve(static_cast<std::size_t>(count));
    for(Integer i=0;i<count;++i) {
        auto row=read(cursor+i); if(!row.ok) return {false,{},row.error};
        if(row.value.id.size()>4096||row.value.kind.size()>4096||row.value.text.size()>4096) return fail("JourneyViewTextLimit");
        page->rows.push_back(std::move(row.value));
    }
    if(cursor+count<total) page->next_cursor=cursor+count;
    return {true,std::move(page),{}};
}
} // cursor/epoch/revision让上层丢弃旧页，不让显示等待触发自动推进。
