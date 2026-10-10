/* Authored Give_Item keycard/Brainstem use the default RF.exe45a4cc path:
 * 45a100 publishes a pickup notice without adding inventory or equipment flags.
 * Include after campaign_pickup_notice_grant and before campaign_apply_item_grant.
 * Negative request tags stay inside the existing ordered startup grant queue;
 * -7 remains the unresolved-name sentinel in campaign_give_item. */
enum { CAMPAIGN_ITEM_KEYCARD=-8, CAMPAIGN_ITEM_BRAINSTEM=-9 };
/* Scripted-only diagnostic kinds, never placed-resource array indices. */
enum { SCENE_SCRIPTED_NOTICE_KEYCARD=0x100, SCENE_SCRIPTED_NOTICE_BRAINSTEM=0x101 };

static int32_t scene_scripted_notice_item_id(const char *name)
{
    if(scene_pickup_name_equal(name,"keycard"))return CAMPAIGN_ITEM_KEYCARD;
    if(scene_pickup_name_equal(name,"Brainstem"))return CAMPAIGN_ITEM_BRAINSTEM;
    return -7;
}

/* Return1 only for a fully handled notice-only request. The caller must return
 * RF_OK before any weapon-definition/inventory indexing for these tags. */
static uint32_t scene_scripted_notice_item_apply(const campaign_item_grant *request)
{
    uint32_t kind;
    if(request->weapon==CAMPAIGN_ITEM_KEYCARD)kind=SCENE_SCRIPTED_NOTICE_KEYCARD;
    else if(request->weapon==CAMPAIGN_ITEM_BRAINSTEM)kind=SCENE_SCRIPTED_NOTICE_BRAINSTEM;
    else return 0;
    campaign_pickup_notice_grant(request->pickup_messages,0,(uint32_t)request->quantity);
    ++rf_scene_nonweapon_items[0];++rf_scene_nonweapon_items[2];rf_scene_nonweapon_items[3]=kind;
    ++rf_scene_script_grants[0];rf_scene_script_grants[3]=(uint32_t)request->weapon;
    rf_scene_script_grants[4]=rf_scene_script_grants[5]=rf_scene_script_grants[6]=rf_scene_script_grants[7]=0;
    return 1;
}
