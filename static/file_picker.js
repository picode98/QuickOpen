(function($)
{
    
    const entryMarkup = `<div class="mfp-entry"><input type="file" class="mfp-input" multiple />
        <button type="button" class="mfp-add-button">Add</button>
        <button type="button" class="mfp-remove-button">Remove</button></div>`;

    function createEntry()
    {
        let entryObj = $(entryMarkup);
        entryObj.children('.mfp-add-button').click(() => {
            entryObj.parent().multiFilePicker('insertEntry', entryObj);
        });
        entryObj.children('.mfp-remove-button').click(() => {
            entryObj.parent().multiFilePicker('removeEntry', entryObj);
        });

        return entryObj;
    }

    $.fn.multiFilePicker = function(action, param2, param3)
    {
        switch (action)
        {
            case undefined:
            case null:
            case '':
            {
                let firstEntry = createEntry();
                firstEntry.children('.mfp-remove-button').hide();
                $(this).append(firstEntry);

                break;
            }
            case 'getFiles':
            {
                let fileList = [];
                $(this).children('.mfp-entry').children('.mfp-input').each(function()
                {
                    fileList = fileList.concat(Array.from(this.files));
                });
                return fileList;
            }
            case 'appendEntry':
            {
                return $(this).multiFilePicker('insertEntry', $(this).children('.mfp-entry:last'), param2);
            }
            case 'insertEntry':
            {
                let newEntry = createEntry();
                let insertAfterNode = param2;
                newEntry.insertAfter(insertAfterNode);
                $(this).children('.mfp-entry:first').children('.mfp-remove-button').show();

                let files = param3;
                if (files && files.length > 0) {
                    newEntry.children('.mfp-input:first')[0].files = files;
                }

                break;
            }
            case 'removeEntry':
            {
                let entryToRemove = $(param2);
                entryToRemove.remove();

                let remainingChildren = $(this).children('.mfp-entry');
                if (remainingChildren.length <= 1)
                {
                    remainingChildren.children('.mfp-remove-button').hide();
                }
                break;
            }
            default:
                throw new Error(`"${action}" is not a valid action.`);
        }
    }
})(jQuery)