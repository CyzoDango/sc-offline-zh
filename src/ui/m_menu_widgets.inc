// M-menu only. The launcher never includes this implementation.
float Ui(float value) { return value * g_menuDpiScale; }
static std::string MElide(const char* text, float width) {
    std::string out=text?text:"";
    if(ImGui::CalcTextSize(out.c_str()).x<=width)return out;
    while(!out.empty()&&ImGui::CalcTextSize((out+"…").c_str()).x>width){
        size_t i=out.size()-1;while(i&&((static_cast<unsigned char>(out[i])&0xc0)==0x80))--i;out.resize(i);
    }return out+"…";
}
static void MTextAt(ImDrawList* d,ImVec2 p,ImU32 color,const char* text,float width,float size=0){
    const std::string clipped=MElide(text,width);
    if(size>0)d->AddText(ImGui::GetFont(),size,p,color,clipped.c_str());else d->AddText(p,color,clipped.c_str());
}
void SectionHeading(const char* title) {
    ImGui::Dummy({0,Ui(8)});ImGui::PushFont(g_menuBold,Ui(12)*kFontEmToPixel);
    ImGui::TextColored(kLeaf,"%s",title);ImGui::PopFont();ImGui::Dummy({0,Ui(1)});
}
void Hint(const char* text){ImGui::PushFont(nullptr,Ui(12)*kFontEmToPixel);ImGui::PushStyleColor(ImGuiCol_Text,kMuted);ImGui::TextWrapped("%s",text);ImGui::PopStyleColor();ImGui::PopFont();}
static bool MVisualButton(const char*label,float height,float radius,float fontSize,ImFont*font,ImVec4 base,ImVec4 hover,ImVec4 text){
    ImGui::PushID(label);ImGui::PushFont(font,Ui(fontSize)*kFontEmToPixel);
    const auto labelSize=ImGui::CalcTextSize(label,nullptr,true);const float w=labelSize.x+Ui(20),h=Ui(height);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    ImGui::PushStyleColor(ImGuiCol_Button,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_Text,{0,0,0,0});
    const bool hit=ImGui::Button("##button",{w,h});const auto a=ImGui::GetItemRectMin();
    const float over=sco_terminal::Motion(ImGui::GetID("hover"),ImGui::IsItemHovered()?1.f:0.f,18),pressed=sco_terminal::Motion(ImGui::GetID("press"),ImGui::IsItemActive()?1.f:0.f,26),scale=1-.06f*pressed;
    ImGui::PopStyleColor(4);ImGui::PopStyleVar(2);auto*d=ImGui::GetWindowDrawList();
    ImVec4 color{base.x+(hover.x-base.x)*over,base.y+(hover.y-base.y)*over,base.z+(hover.z-base.z)*over,base.w+(hover.w-base.w)*over};
    const ImVec2 center{a.x+w*.5f,a.y+h*.5f};d->AddRectFilled({center.x-w*scale*.5f,center.y-h*scale*.5f},{center.x+w*scale*.5f,center.y+h*scale*.5f},ImGui::GetColorU32(color),Ui(radius)*scale);
    MTextAt(d,{center.x-labelSize.x*.5f,center.y-labelSize.y*.5f},ImGui::GetColorU32(text),label,w-Ui(12));
    if(ImGui::IsItemFocused())d->AddRect(a,{a.x+w,a.y+h},ImGui::GetColorU32(kLeaf),Ui(radius));ImGui::PopFont();ImGui::PopID();return hit;
}
bool MButton(const char* label,bool secondary){return MVisualButton(label,26,8,12,g_menuBold,Hex(secondary?0xE5F2F0:0x127A72),Hex(secondary?0xD4EBE8:0x0E6B64),Hex(secondary?0x127A72:0xFFFFFF));}
bool PrimaryButton(const char* label,float){return MButton(label);}
bool MChip(const char*label,bool selected){return MVisualButton(label,28,14,13,selected?g_menuBold:nullptr,Hex(selected?0x127A72:0xE5F2F0),Hex(selected?0x0E6B64:0xD4EBE8),Hex(selected?0xFFFFFF:0x127A72));}
bool MBeginCombo(const char* id,const char* preview,ImGuiComboFlags flags){
    const ImVec2 p=ImGui::GetCursorScreenPos();const float width=ImGui::CalcItemWidth(),height=ImGui::GetFrameHeight();
    auto*d=ImGui::GetWindowDrawList();const std::string label=MElide(preview,width-Ui(36));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{Ui(8),Ui(8)});
    const bool open=ImGui::BeginCombo(id,label.c_str(),flags|ImGuiComboFlags_NoArrowButton);ImGui::PopStyleVar();
    const ImVec2 c{p.x+width-Ui(17),p.y+height*.5f};const auto color=ImGui::GetColorU32(kLeaf);
    d->AddLine({c.x-Ui(4),c.y-Ui(2)},{c.x,c.y+Ui(2)},color,Ui(1.5));d->AddLine({c.x,c.y+Ui(2)},{c.x+Ui(4),c.y-Ui(2)},color,Ui(1.5));
    return open;
}
bool MPopup(const char*id){
    // The native overlay is only the menu's size, not the game desktop.
    // Anchor inside that viewport instead of opening at a right-hand button.
    const auto* viewport=ImGui::GetMainViewport();
    const float width=(std::min)(Ui(520),(std::max)(Ui(1),viewport->WorkSize.x-Ui(48)));
    const float height=(std::min)(Ui(460),(std::max)(Ui(1),viewport->WorkSize.y-Ui(48)));
    ImGui::SetNextWindowSizeConstraints({width,0},{width,height});
    ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x*.5f,
                             viewport->WorkPos.y+viewport->WorkSize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{Ui(12),Ui(12)});
    const bool open=ImGui::BeginPopup(id,ImGuiWindowFlags_NoMove);ImGui::PopStyleVar();return open;
}
bool MCombo(const char* id,int* value,const char*const* items,int count){
    if(count<=0)return false;*value=std::clamp(*value,0,count-1);bool changed=false;
    if(MBeginCombo(id,items[*value])){for(int i=0;i<count;++i){
        if(V3Selectable(items[i],i==*value,i)){*value=i;changed=true;}if(i==*value)ImGui::SetItemDefaultFocus();
    }ImGui::EndCombo();}return changed;
}
bool MTextField(const char*id,const char*hint,char*buffer,size_t size,ImGuiInputTextFlags flags){
    return ImGui::InputTextWithHint(id,hint,buffer,size,flags);
}
bool SearchBox(const char*id,const char*hint,char*buffer,size_t size){
    ImGui::SetNextItemWidth(-1);const bool changed=MTextField(id,hint,buffer,size);ImGui::Dummy({0,Ui(4)});return changed;
}
void MListHeader(const char* name,const char* category){
    auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x;auto*d=ImGui::GetWindowDrawList();
    const auto c=ImGui::GetColorU32(Hex(0x9A978E));ImGui::PushFont(nullptr,Ui(11)*kFontEmToPixel);
    d->AddText({p.x+Ui(8),p.y+Ui(3)},c,name);d->AddText({p.x+w-Ui(230),p.y+Ui(3)},c,category);
    ImGui::PopFont();ImGui::Dummy({w,Ui(22)});
}
bool MActionRow(const char*id,const char*name,const char*category,const char*action,bool selected,bool*picked,int row,const char*tip){
    ImGui::PushID(id);const auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x,h=Ui(38);
    if(!ImGui::IsRectVisible({w,h})){if(picked)*picked=false;ImGui::Dummy({w,h});ImGui::PopID();return false;}
    const float aw=action?ImGui::CalcTextSize(action).x*(12.f/13.f)+Ui(20):0;
    const bool hovered=ImGui::IsMouseHoveringRect(p,{p.x+w,p.y+h});
    const float fade=sco_terminal::Motion(ImGui::GetID("hover"),hovered?1.f:0.f,18);
    const float t=std::clamp(float((ImGui::GetTime()-g_v3PageSince-double(std::clamp(row-int(ImGui::GetScrollY()/Ui(42)),0,8))*.03)/.22),0.f,1.f),e=1-powf(1-t,3);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*e);const float rise=(1-e)*Ui(6);
    auto*d=ImGui::GetWindowDrawList();const float opacity=selected?.08f:.08f*fade;
    d->AddRectFilled({p.x,p.y+rise},{p.x+w,p.y+h+rise},ImGui::GetColorU32(Hex(0x127A72,ImGui::IsMouseDown(0)&&hovered?.14f:opacity)),Ui(10));
    ImGui::SetCursorScreenPos(p);const bool clicked=ImGui::InvisibleButton("select",{(std::max)(Ui(1),w-aw-Ui(16)),h});if(picked)*picked=clicked;
    const float catX=p.x+w-Ui(230);MTextAt(d,{p.x+Ui(8),p.y+(h-ImGui::GetFontSize())*.5f+rise},ImGui::GetColorU32(ImGuiCol_Text),name,(std::max)(Ui(20),catX-p.x-Ui(20)));
    ImGui::PushFont(nullptr,Ui(12)*kFontEmToPixel);MTextAt(d,{catX,p.y+(h-ImGui::GetFontSize())*.5f+rise},ImGui::GetColorU32(kMuted),category,Ui(145));ImGui::PopFont();
    bool fire=false;if(action){ImGui::SetCursorScreenPos({p.x+w-aw-Ui(8),p.y+Ui(6)+rise});fire=MButton(action);}
    if(hovered&&tip&&*tip)ImGui::SetTooltip("%s",tip);
    ImGui::SetCursorScreenPos(p);ImGui::Dummy({w,h});ImGui::PopStyleVar();ImGui::PopID();return fire;
}
bool MSetting(const char*label,const char*description,bool*value){
    const auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x,h=Ui(description&&*description?44.f:40.f);
    ImGui::PushID(label);const auto id=ImGui::GetID("toggle");const bool hit=ImGui::InvisibleButton("toggle",{w,h},ImGuiButtonFlags_EnableNav);
    if(hit){*value=!*value;ImGui::MarkItemEdited(id);}
    auto*storage=ImGui::GetStateStorage();float x=storage->GetFloat(id+91,*value?1.f:0.f),v=storage->GetFloat(id+92,0);
    const float dt=(std::min)(ImGui::GetIO().DeltaTime,.05f);v+=(28*28*((*value?1.f:0.f)-x)-56*v)*dt;x+=v*dt;
    storage->SetFloat(id+91,x);storage->SetFloat(id+92,v);const float t=std::clamp(x,0.f,1.f);
    auto*d=ImGui::GetWindowDrawList();if(ImGui::IsItemHovered())d->AddRectFilled(p,{p.x+w,p.y+h},ImGui::GetColorU32(Hex(0x127A72,.07f)),Ui(10));
    MTextAt(d,{p.x+Ui(8),p.y+(description&&*description?Ui(3):(h-ImGui::GetFontSize())*.5f)},ImGui::GetColorU32(ImGuiCol_Text),label,w-Ui(70));
    if(description&&*description){ImGui::PushFont(nullptr,Ui(11)*kFontEmToPixel);MTextAt(d,{p.x+Ui(8),p.y+Ui(24)},ImGui::GetColorU32(kMuted),description,w-Ui(70));ImGui::PopFont();}
    const ImVec2 a{p.x+w-Ui(44),p.y+(h-Ui(20))*.5f};const auto off=Hex(0xD4D1C9),on=kLeaf;
    d->AddRectFilled(a,{a.x+Ui(36),a.y+Ui(20)},ImGui::GetColorU32({off.x+(on.x-off.x)*t,off.y+(on.y-off.y)*t,off.z+(on.z-off.z)*t,1}),Ui(6));
    const ImVec2 knob{a.x+Ui(3+16*t),a.y+Ui(3)};d->AddRectFilled(knob,{knob.x+Ui(14),knob.y+Ui(14)},ImGui::GetColorU32(Hex(0xFFFFFF)),Ui(4));
    d->AddRect(knob,{knob.x+Ui(14),knob.y+Ui(14)},ImGui::GetColorU32(Hex(0x1C1C1A,.12f)),Ui(4));
    if(ImGui::IsItemHovered()&&description&&*description)ImGui::SetTooltip("%s",description);
    if(ImGui::IsItemFocused())d->AddRect(p,{p.x+w,p.y+h},ImGui::GetColorU32(kLeaf),Ui(10));ImGui::PopID();return hit;
}
bool V3Toggle(const char*label,bool*value){return MSetting(label,nullptr,value);}
bool V3Selectable(const char*label,bool selected,int,ImGuiSelectableFlags flags){
    const auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x,h=Ui(30);
    const bool over=ImGui::IsMouseHoveringRect(p,{p.x+w,p.y+h});auto*d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,{p.x+w,p.y+h},ImGui::GetColorU32(Hex(0x127A72,selected||over?.08f:0)),Ui(8));
    ImGui::PushStyleColor(ImGuiCol_Header,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_HeaderHovered,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_HeaderActive,{0,0,0,0});ImGui::PushStyleColor(ImGuiCol_Text,{0,0,0,0});
    const bool hit=ImGui::Selectable(label,selected,flags,{w,h});ImGui::PopStyleColor(4);
    const std::string text(label,ImGui::FindRenderedTextEnd(label));MTextAt(d,{p.x+Ui(8),p.y+(h-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(kText),text.c_str(),w-Ui(16));return hit;
}
template<class T>static bool MSlider(const char*label,T*value,T lo,T hi,const char*format,ImGuiSliderFlags flags){
    ImGui::PushID(label);auto p=ImGui::GetCursorScreenPos();const float w=ImGui::GetContentRegionAvail().x,h=Ui(38),trackW=Ui(148);
    const ImVec2 start{p.x+w-Ui(156),p.y+Ui(9)};ImGui::SetCursorScreenPos(start);ImGui::SetNextItemWidth(trackW);
    const bool editing=ImGui::TempInputIsActive(ImGui::GetID("value"));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,Hex(0,0));ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,Hex(0,0));ImGui::PushStyleColor(ImGuiCol_FrameBgActive,Hex(0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,Hex(0,0));ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,Hex(0,0));if(!editing)ImGui::PushStyleColor(ImGuiCol_Text,Hex(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,Ui((20-13*kFontEmToPixel)*.5f)});ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    bool changed;if constexpr(std::is_same_v<T,int>)changed=ImGui::SliderInt("value",value,lo,hi,format,flags);else changed=ImGui::SliderFloat("value",value,lo,hi,format,flags);
    ImGui::PopStyleVar(2);ImGui::PopStyleColor(editing?5:6);
    auto*d=ImGui::GetWindowDrawList();const char*end=ImGui::FindRenderedTextEnd(label);std::string title(label,end);MTextAt(d,{p.x+Ui(8),p.y+(h-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(ImGuiCol_Text),title.c_str(),w-Ui(260));
    char number[128];snprintf(number,sizeof(number),format,*value);ImGui::PushFont(nullptr,Ui(12)*kFontEmToPixel);MTextAt(d,{start.x-Ui(100),p.y+(h-ImGui::GetFontSize())*.5f},ImGui::GetColorU32(kMuted),number,Ui(92));ImGui::PopFont();
    if(!editing){float t=float(*value-lo)/float(hi-lo);if((flags&ImGuiSliderFlags_Logarithmic)&&lo>0)t=logf(float(*value)/float(lo))/logf(float(hi)/float(lo));t=std::clamp(t,0.f,1.f);
        const float y=start.y+Ui(10),x=start.x+t*trackW;d->AddRectFilled({start.x,y-Ui(1)},{start.x+trackW,y+Ui(1)},ImGui::GetColorU32(Hex(0xE2E0DA)),Ui(1));d->AddRectFilled({start.x,y-Ui(1)},{x,y+Ui(1)},ImGui::GetColorU32(kLeaf),Ui(1));d->AddCircleFilled({x,y},Ui(7),ImGui::GetColorU32(Hex(0xFFFFFF)));d->AddCircle({x,y},Ui(7),ImGui::GetColorU32(kLeaf),0,Ui(2));}
    ImGui::SetCursorScreenPos(p);ImGui::Dummy({w,h});ImGui::PopID();return changed;
}
bool MSliderFloat(const char*l,float*v,float a,float b,const char*f,ImGuiSliderFlags flags){return MSlider(l,v,a,b,f,flags);}
bool MSliderInt(const char*l,int*v,int a,int b,const char*f){return MSlider(l,v,a,b,f,0);}
float Columns(int n){return (ImGui::GetContentRegionAvail().x-(n-1)*ImGui::GetStyle().ItemSpacing.x)/n;}
