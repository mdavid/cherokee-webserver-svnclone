var groupN = 0;
var ruleN = 0;
var enSaveRules = false;

function addGlobal(cond) {
    ghtml = 'If <select class="noautosubmit" id="group_all" name="group_all"><option value="or">any</option><option value= "and">all</option></select> of the following groups match:';
    $("#rule_all").html(ghtml);
    $("#group_all")
        .val(cond)
        .change(saveRules);
}

function addGroup(group, cond) {
    groupN = group

    ghtml = '<div class="group_cond_div">If <select class="noautosubmit" id="cond_rule_group' + groupN + '"><option value="or">any</option><option value="and">all</option></select> of the following conditions are met:</div>';

    $("<div/>")
        .attr("id", "rule_group" + groupN)
        .addClass("tableprop_block rules_group")
        .html(ghtml)
        .appendTo("#rules_area");

    $("#cond_rule_group" + groupN)
        .val(cond)
        .change(saveRules);

    $("<div/>")
        .attr("id", "rule_group_div" + groupN)
        .addClass("rules_group_inner")
        .appendTo("#rule_group" + groupN);

    if ($(".rules_group").size() > 1) {
        if ($("#group_all").length == 0) {
            addGlobal('or');
        }
    }

    if ($("#addnewgroup").length) {
        $("#addnewgroup").appendTo("#rule_group" + groupN);
    } else {
        $("<div/>")
            .attr("id", "addnewgroup")
            .appendTo("#rule_group" + groupN);
    
        $("<a/>")
            .click(function () { 
                addRule(addGroup(groupN + 1, 'or'), 0);
                return false;
            })
            .html('Add new rules group')
            .appendTo("#addnewgroup");
    }

    return groupN;
}



function addRule(group, after, ruleData) {
    ruleN++;
    rid = "_r" + group + "_" + ruleN;
    locRule = ruleN;

    if (after == 0) {
        $("<table/>")
            .attr("id", "table" + rid)
            .addClass("tableprop")
            .appendTo("#rule_group_div" + group);
    } else {
        $("<table/>")
            .attr("id", "table" + rid)
            .addClass("tableprop")
            .insertAfter("#hr_r" + group + "_" + after);
    }

    $("<tr/>")
        .attr("id", "row" + rid)
        .appendTo("#table" + rid);
    $("<td/>")
        .attr("id", "tdrule" + rid)
        .addClass("rule_dd")
        .appendTo("#row" + rid);
    $("<td/>")
        .attr("id", "tdcond" + rid)
        .addClass("rule_condition")
        .appendTo("#row" + rid);
    $("<td/>")
        .attr("id", "tdval" + rid)
        .addClass("rule_field")
        .appendTo("#row" + rid);
    $("<td/>")
        .attr("id", "tdcont" + rid)
        .addClass("rule_control")
        .appendTo("#row" + rid);
    $("<tr/>")
        .attr("id", "rowhint" + rid)
        .appendTo("#table" + rid);
    $("<td/>")
        .attr({
            "id": "tdhint" + rid,
            "colspan": "4"
        })
        .appendTo("#rowhint" + rid);
    $("<div/>")
        .attr("id", "hint" + rid)
        .addClass("comment")
        .appendTo("#tdhint" + rid);
    $("<hr/>")
        .attr("id", "hr" + rid)
        .insertAfter("#table" + rid);

    $("<select/>")
        .attr("id", rid)
        .addClass("noautosubmit in_rule_group" + group)
        .bind(
            'change',
            { "el": rid }, 
            function(event) { 
                getFields(event.data.el);
            }
        )
        .appendTo("#tdrule" + rid);

    for (var i in cherokeeRules) {
        $("<option/>")
            .html(cherokeeRules[i].desc)
            .attr("value", i)
            .appendTo("#" + rid);
    }

    if (ruleData !== undefined) {
        if (ruleData[0] !== undefined) $("#" + rid).val(ruleData[0]);
    }

    getFields(rid, ruleData); 

    // Add button
    $("<a/>")
        .attr({
            "id": "add"+rid,
            "title": "Add new rule"
        })
        .bind(
            'click',
            { "g": group, "r": ruleN },
            function(event) {
                addRule(event.data.g, event.data.r);
                return false;
            }
        )
        .appendTo("#tdcont" + rid);

    $("<img/>")
        .attr("src", "/static/images/add.png")
        .appendTo("#add" + rid);

    // Remove button
    $("<a/>")
        .attr({
            "id": "del"+rid,
            "title": "Remove rule"
        })
        .bind(
            'click',
            { "g": group, "r": ruleN },
            function(event) {
                delRule(event.data.g, event.data.r);
                return false;
            }
        )
        .appendTo("#tdcont" + rid);

    $("<img/>")
        .attr("src", "/static/images/rem.png")
        .appendTo("#del" + rid);

}

function delRule(group, rule)
{
    rid = "_r" + group + "_" + rule;
    if ($(".in_rule_group"+group).size() > 1) {
        $("#table"+rid).remove();
        $("#hr"+rid).remove();
    } else if ($(".rules_group").size() > 1) {
        if ($("#rule_group"+group+" > #addnewgroup").length) {
            $("#addnewgroup").appendTo($("#rule_group"+group).prev());
        }
        $("#rule_group"+group).remove();
    } else {
        alert ("Must have at least one rule");
    }
    if ($(".rules_group").size() == 1) {
        $("#rule_all").html('');
    }
    saveRules();
}


function getFields(el, ruleData) {
    var data = cherokeeRules[$("#"+el).val()];
            // Conditions
            $("#tdcond" + el).empty();
            if (!data.conditions) {
                $("#tdcond" + el).attr("style", "width: 0;");
                $("#hint" + el).html(data.hint);
            } else {
                $("#tdcond" + el).attr("style", "width: 10em;");
                $("<select/>")
                    .attr("id", "cond" + el)
                    .appendTo("#tdcond" + el)
                    .addClass("noautosubmit")
                    .change(
                        function() { 
                            saveRules(); 
                            $("#hint" + el).html(data.conditions[this.value].h); 
                        });

                $.each(data.conditions, function(i,item) {
                    $("<option/>")
                        .attr("value", i)
                        .html(item.d)
                        .appendTo("#cond" + el);
                });
        
                if ((ruleData !== undefined) && 
                    (ruleData[1] !== undefined)) {
                    $("#cond" + el).val(ruleData[1]);
                }
                
                $("#hint" + el).html(data.conditions[$("#cond" + el).val()].h);
            }
            // Field
            $("#tdval" + el).empty();
            switch (data.field.type) {
                case 'entry':
                    $('<input type="text"/>')
                        .attr("id", "f" + el)
                        .addClass("noautosubmit")
                        .change(saveRules)
                        .appendTo("#tdval" + el);

                    break;
                case 'dropdown':
                    $("<select/>")
                        .attr("id", "f" + el)
                        .change(saveRules)
                        .addClass("noautosubmit")
                        .appendTo("#tdval" + el)
                    $.each(data.field.choices, function(i,item) {
                        $("<option/>")
                            .attr("value", i)
                            .html(item)
                            .appendTo("#f" + el);
                    });

                    break;
            }
            if ((ruleData !== undefined) && 
                (ruleData[2] !== undefined)) {
                $("#f" + el).val(ruleData[2]);
            }
            saveRules();
}

function enableSaveRules()
{
    enSaveRules = true
}

function disableSaveRules()
{
    enSaveRules = false
}



function saveRules() 
{
    if (!enSaveRules) return false;

    var ret = {};

    var ng = 0;
    ret['save_rules'] = 'true';
    ret['pre'] = cherokeePre;

    if ($('#group_all').val() !== undefined) {
        ret[cherokeePre + '!match'] = $("#group_all").val();
    } else {
        ret[cherokeePre + '!match'] = '';
    }

    $(".rules_group").each(function(i){
        ret[cherokeePre + '!match!'+ng] = $('#cond_'+this.id).val();

        // Rules
        var nr = 0;
        $(".in_"+this.id).each(function(i){
            if ($('#'+this.id).val() !== undefined) {
                ret[cherokeePre + '!match!'+ng+'!'+nr] = $('#'+this.id).val();
            }
            if ($('#cond'+this.id).val() !== undefined) {
                ret[cherokeePre + '!match!'+ng+'!'+nr+'!cond'] = $('#cond'+this.id).val();
            }
            if ($('#f'+this.id).val() !== undefined) {
                ret[cherokeePre + '!match!'+ng+'!'+nr+'!val'] = $('#f'+this.id).val();
            }
            nr ++;
        });

        ng++;
    });

    $.post('/ajax/update', 
            ret,
            function(data) {
                //$("#savePre").html(data);
            });
}
