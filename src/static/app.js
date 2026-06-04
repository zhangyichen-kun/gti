/* ============================================
   CloudDisk 云盘 - 前端逻辑
   ============================================ */

// ---- DOM 元素 ----
const uploadBtn   = document.getElementById('uploadBtn');
const fileInput   = document.getElementById('fileInput');
const fileGrid    = document.getElementById('fileGrid');
const emptyState  = document.getElementById('emptyState');
const statsBar    = document.getElementById('statsBar');
const fileCountEl = document.getElementById('fileCount');
const progressBar = document.getElementById('progressBar');
const progressFill = document.querySelector('.progress-fill');
const progressText = document.querySelector('.progress-text');

// 预览模态框
const previewModal  = document.getElementById('previewModal');
const previewImage  = document.getElementById('previewImage');
const previewName   = document.getElementById('previewName');
const previewSize   = document.getElementById('previewSize');
const modalClose    = document.querySelector('.modal-close');
const modalBackdrop = document.querySelector('.modal-backdrop');

// ---- 初始化 ----
document.addEventListener('DOMContentLoaded', () => {
    loadFiles();

    // 上传按钮 → 触发文件选择
    uploadBtn.addEventListener('click', () => fileInput.click());
    fileInput.addEventListener('change', handleUpload);

    // 预览关闭
    modalClose.addEventListener('click', closePreview);
    modalBackdrop.addEventListener('click', closePreview);
    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') closePreview();
    });
});

// ====================================================
// 加载文件列表
// ====================================================
async function loadFiles() {
    try {
        const resp = await fetch('/api/files');
        const files = await resp.json();

        if (files.length === 0) {
            emptyState.classList.remove('hidden');
            statsBar.classList.add('hidden');
            fileGrid.innerHTML = '';
            return;
        }

        emptyState.classList.add('hidden');
        statsBar.classList.remove('hidden');
        fileCountEl.textContent = `共 ${files.length} 张图片`;

        fileGrid.innerHTML = files.map(f => createCard(f)).join('');

        // 绑定卡片事件
        bindCardEvents();

    } catch (err) {
        console.error('加载文件列表失败:', err);
    }
}

// ====================================================
// 生成文件卡片 HTML
// ====================================================
function createCard(file) {
    return `
        <div class="file-card" data-id="${file.id}" data-name="${file.filename}">
            <div class="card-thumb" data-action="preview">
                <img src="/api/image/${file.id}"
                     alt="${escapeHtml(file.filename)}"
                     loading="lazy">
            </div>
            <div class="card-body">
                <div class="card-name" title="${escapeHtml(file.filename)}">
                    ${escapeHtml(file.filename)}
                </div>
                <div class="card-meta">
                    <span>${file.size_display}</span>
                    <span>${file.upload_time}</span>
                </div>
                <div class="card-actions">
                    <button class="btn-sm btn-download" data-action="download">
                        下载
                    </button>
                    <button class="btn-sm btn-delete" data-action="delete">
                        删除
                    </button>
                </div>
            </div>
        </div>`;
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ====================================================
// 绑定卡片事件 (事件代理)
// ====================================================
function bindCardEvents() {
    fileGrid.addEventListener('click', (e) => {
        const card = e.target.closest('.file-card');
        if (!card) return;

        const id   = card.dataset.id;
        const name = card.dataset.name;
        const action = e.target.dataset.action
                    || e.target.closest('[data-action]')?.dataset.action;

        if (action === 'download') {
            downloadFile(id, name);
        } else if (action === 'delete') {
            deleteFile(id, card);
        } else if (action === 'preview' || e.target.closest('.card-thumb')) {
            previewFile(id, name);
        }
    });
}

// ====================================================
// 处理上传
// ====================================================
async function handleUpload() {
    const files = fileInput.files;
    if (files.length === 0) return;

    for (const file of files) {
        // 只允许图片
        if (!file.type.startsWith('image/')) {
            alert(`"${file.name}" 不是图片文件，已跳过`);
            continue;
        }

        const formData = new FormData();
        formData.append('file', file);

        // 显示进度条
        progressBar.classList.remove('hidden');
        progressFill.style.width = '0%';
        progressText.textContent = `正在上传: ${file.name}`;

        try {
            await new Promise((resolve, reject) => {
                const xhr = new XMLHttpRequest();

                xhr.upload.addEventListener('progress', (e) => {
                    if (e.lengthComputable) {
                        const pct = Math.round((e.loaded / e.total) * 100);
                        progressFill.style.width = pct + '%';
                    }
                });

                xhr.addEventListener('load', () => {
                    if (xhr.status >= 200 && xhr.status < 300) {
                        resolve(JSON.parse(xhr.responseText));
                    } else {
                        reject(new Error(xhr.responseText));
                    }
                });

                xhr.addEventListener('error', () => reject(new Error('网络错误')));
                xhr.open('POST', '/api/upload');
                xhr.send(formData);
            });

        } catch (err) {
            console.error('上传失败:', err);
            alert(`上传 "${file.name}" 失败`);
        }
    }

    // 隐藏进度条
    progressBar.classList.add('hidden');
    progressFill.style.width = '0%';

    // 清空选择
    fileInput.value = '';

    // 刷新列表
    loadFiles();
}

// ====================================================
// 下载文件
// ====================================================
function downloadFile(id, filename) {
    const a = document.createElement('a');
    a.href = `/api/download/${id}`;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
}

// ====================================================
// 删除文件
// ====================================================
async function deleteFile(id, cardElement) {
    if (!confirm('确定要删除这张图片吗？此操作不可恢复。')) return;

    try {
        const resp = await fetch(`/api/files/${id}`, {
            method: 'DELETE'
        });

        if (resp.ok) {
            // 删除卡片动画
            cardElement.style.transition = 'opacity 0.3s, transform 0.3s';
            cardElement.style.opacity = '0';
            cardElement.style.transform = 'scale(0.8)';

            setTimeout(() => {
                cardElement.remove();
                // 检查是否为空
                if (fileGrid.children.length === 0) {
                    emptyState.classList.remove('hidden');
                    statsBar.classList.add('hidden');
                } else {
                    fileCountEl.textContent = `共 ${fileGrid.children.length} 张图片`;
                }
            }, 300);

        } else {
            const err = await resp.json();
            alert('删除失败: ' + (err.error || '未知错误'));
        }

    } catch (err) {
        console.error('删除失败:', err);
        alert('删除失败，请重试');
    }
}

// ====================================================
// 图片预览
// ====================================================
function previewFile(id, filename) {
    previewImage.src = `/api/image/${id}`;
    previewName.textContent = filename;
    previewSize.textContent = '';  // 可扩展
    previewModal.classList.remove('hidden');
}

function closePreview() {
    previewModal.classList.add('hidden');
    previewImage.src = '';
}
