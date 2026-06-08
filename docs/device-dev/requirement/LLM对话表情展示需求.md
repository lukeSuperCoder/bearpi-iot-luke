# LLM对话表情展示需求
## 新增一个示例，功能如下： 
  1. 在claude cli对话中新增一个hook，每次回复内容时要向开发板发送一条消息代码                                 
  2. 这个消息代码就是把当前的对话的内容通过汇总成一个emoji表情符号，要求必须是tools\emoji_wifi_controller.html示例文件中的20个表情中的一个，比如项目运行成功就展示开心表情，失败或报错就显示难过表情等
  3. 通过llm-api+prompt来汇总内容，汇总完成之后通过ws或者其他方式发送到开发板上，开发板通过lcd展示对应的表情
  4. 在applications\C2_wifi_emoji示例基础上开发
  5. llm api 接口调用示例
  ```
  curl --request POST \
  --url https://open.bigmodel.cn/api/paas/v4/chat/completions \
  --header 'Authorization: Bearer <token>' \
  --header 'Content-Type: application/json' \
  --data '
{
  "model": "glm-5.1",
  "messages": [
    {
      "role": "system",
      "content": "你是一个有用的AI助手。"
    },
    {
      "role": "user",
      "content": "请介绍一下人工智能的发展历程。"
    }
  ],
  "stream": false,
  "temperature": 1
}
'
  ```
  6. api key 通过配置文件我来配置